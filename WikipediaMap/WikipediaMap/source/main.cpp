#include"imgui.h"
#include"imgui_impl_glfw.h"
#include"imgui_impl_opengl3.h"

#include <../header/config.h>
#include <../header/shader.h>
#include <chrono>
#include <vector>
#include <string>
#include <random>
#include <fstream>
#include <thread>


// The template for quads drawn
GLfloat vertices[] = {
	-0.05f, -0.05f, 0.0f, // Bottom left corner
	-0.05f, 0.05f, 0.0f, // Top left corner
	0.05f, 0.05f, 0.0f, // Top right corner
	-0.05f, -0.05f, 0.0f, // Bottom left corner
	0.05f, -0.05f, 0.0f, // Bottom right corner
	0.05f, 0.05f, 0.0f, // Top right corner
};


// Variables
glm::vec2* nodeTranslations;
glm::vec2* lineTranslations;
glm::vec2* cpuNodeTranslations;
std::vector<std::string> names;
std::vector<std::string> lowercaseNames;
std::vector<std::vector<int>> paths;
int* linksFrom;
int* highlighted;
glm::vec2* links;

int fps = 0;
static std::string fpsText;
static std::string noNodes;
static std::string noConnections;

bool isOpen = true;
ImVec2 infoTooltipOffset = ImVec2(1, 1);

double lastRelativeCursorX = 0.0f;
double lastRelativeCursorY = 0.0f;
double relativeCursorX = 0.0;
double relativeCursorY = 0.0;

bool getInfo = false;
static char articleNameBuf[256] = "";
static char searchBuf[256] = "";
static std::string logText = "";

float zoom = 1.0f;
float scrollSpeed = 0.1f;
bool panning = false;
float panSpeedX = 2.0f / width;
float panSpeedY = 2.0f / height;
glm::vec2 camPos = glm::vec2(0.0f);

bool enqueueGenerateMap = false;
bool hasGeneratedMap = false;
bool generatingMap = false;
bool finishedWikipediaSearch = false;
bool failedWikipediaSearch = false;
bool memoryAllocated = false;
bool memoryAllocationFailure = false;
bool simFrozen = false;
bool searching = false;

int searchDepth = 3;
int linksPerArticle = 5;
int numNames;
int numLinks;

ImGuiWindowFlags popupFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs 
| ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing;
GLuint numWorkGroups = 0;
GLuint posBuffer, linkBuffer, lineBuffer;
bool updateHighlightBufferData = false;
std::chrono::steady_clock::time_point startGeneratingTime;
std::chrono::steady_clock::time_point lastTime;
std::chrono::steady_clock::time_point currentTime;
std::thread generateThread;
std::thread allocateMemory;


// Calculates the framerate
// Modified from https://stackoverflow.com/questions/5627803/how-to-calculate-fps-in-opengl
void CalculateFrameRate() {
	static float framesPerSecond = 0.0f;
	static float lastTimeSecs = 0.0f;
	float currentTimeSecs = GetTickCount64() * 0.001f;
	++framesPerSecond;

	if (currentTimeSecs - lastTimeSecs > 1.0f) {
		lastTimeSecs = currentTimeSecs;
		fps = (int)framesPerSecond;
		fpsText = "FPS: " + std::to_string(fps);
		framesPerSecond = 0;
	}
}


// Gets the current time as a string in H:M:S format for logging
std::string GetCurrentTimeString() {
	time_t now = time(0);
	struct tm timeinfo;
	localtime_s(&timeinfo, &now); // Use localtime_s for safer handling

	char buffer[80];
	strftime(buffer, sizeof(buffer), "%H:%M:%S", &timeinfo);

	return std::string(buffer);
}


// Uses iteration to track an article back to the origin
// and return the indices of the articles it crosses in the process
std::vector<int> GetPath(int startIdx) {
	std::vector<int> path;
	int currentIdx = startIdx;

	while (currentIdx >= 0) {
		path.push_back(currentIdx);
		currentIdx = linksFrom[currentIdx];
	}

	return path;
}


// Displays a window with some simple statistics, like
// the number of nodes, number of connections, and FPS.
void DrawStatsWindow() {
	ImGui::Begin("Info");
	ImGui::Text("%s", fpsText.c_str());
	ImGui::Text("%s", noNodes.c_str());
	ImGui::Text("%s", noConnections.c_str());
	ImGui::Separator();
	ImGui::Text("[I] Show article labels");
	ImGui::End();
}


// Adds a new line of text to the log, beginning it with the current time.
void AddToLog(const char* text) {
	logText += "[" + GetCurrentTimeString() + "]  " + std::string(text) + "\n";
}


// Allocates the necessary memory for the arrays
void AllocateMemory() {
	nodeTranslations = (glm::vec2*)malloc(MAX_NODES * sizeof(*nodeTranslations));
	if (nodeTranslations == nullptr) {
		std::cout << "Memory allocation error - node translations" << std::endl;
		memoryAllocationFailure = true;
		return;
	}
	lineTranslations = (glm::vec2*)malloc(2 * MAX_NODES * sizeof(*lineTranslations));
	if (lineTranslations == nullptr) {
		std::cout << "Memory allocation error - line translations" << std::endl;
		memoryAllocationFailure = true;
		return;
	}
	links = (glm::vec2*)malloc(MAX_NODES * sizeof(*links));
	if (links == nullptr) {
		std::cout << "Memory allocation error - links" << std::endl;
		memoryAllocationFailure = true;
		return;
	}
	highlighted = (int*)malloc(MAX_NODES * sizeof(*highlighted));
	if (highlighted == nullptr) {
		std::cout << "Memory allocation error - highlighted articles" << std::endl;
		memoryAllocationFailure = true;
		return;
	}
	linksFrom = (int*)malloc(MAX_NODES * sizeof(*linksFrom));
	if (linksFrom == nullptr) {
		std::cout << "Memory allocation error - links from list" << std::endl;
		memoryAllocationFailure = true;
		return;
	}
	memoryAllocated = true;
}


// Populate buffers with data and set starting positions of articles
void FillNodes() {
	std::random_device rd;
	std::uniform_real_distribution<float> angle(0.0f, 2 * 3.14159f);
	std::uniform_real_distribution<float> dist(0.0f, 2.0f);

	// fill line translations and node translations with arbitrary starting values
	// (line translations shouldn't all start at (0,0) or else the compute shader may
	// have unintended behavior).
	for (int i = 0; i < numNames; i++) {
		float theta = angle(rd);
		float r = dist(rd);
		nodeTranslations[i] = glm::vec2(r * cos(theta), r * sin(theta));
	}

	for (int i = 0; i < numLinks * 2; i++) {
		lineTranslations[i] = glm::vec2(0, 0);
	}

	// Store the new node translation array in the posBuffer
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, posBuffer);
	glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(glm::vec2) * numNames, nodeTranslations, GL_DYNAMIC_DRAW);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

	// Generate the linkBuffer for keeping track of links in the compute shader
	glGenBuffers(1, &linkBuffer);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, linkBuffer);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, linkBuffer);
	glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(glm::vec2) * numLinks, links, GL_DYNAMIC_COPY);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

	// Generate the lineBuffer with lineTranslations, which will later be used as
	// offsets for the vertices of lines being drawn between articles
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, lineBuffer);
	glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(glm::vec2) * 2 * numLinks, lineTranslations, GL_DYNAMIC_DRAW);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

	// Bind each buffer to their respective binding
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, posBuffer);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, lineBuffer);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, linkBuffer);

	AddToLog(std::string("Added " + std::to_string(numNames) + " nodes.").c_str());
	AddToLog(std::string("Added " + std::to_string(numLinks) + " links.").c_str());
}


// Loads data into local arrays from whatever is in the output text files.
void LoadData() {
	std::ifstream file("source/py/output.txt");
	if (!file.is_open()) {
		std::cerr << "Error opening output file." << std::endl;
		return;
	}

	lowercaseNames.clear();
	numWorkGroups = 0;
	numNames = 0;
	std::string line;
	while (std::getline(file, line)) {
		if (numNames >= MAX_NODES - 1) {
			AddToLog(std::string("Maximum articles reached. Capped at " + std::to_string(numNames)).c_str());
			break;
		}
		// If there was an error or no articles were found, mark the search as failed.
		if (line == "NOARTICLESFOUND!") {
			hasGeneratedMap = false;
			generatingMap = false;
			AddToLog(std::string("No article called '" + std::string(articleNameBuf) + "' was found.").c_str());
			failedWikipediaSearch = true;
			return;
		}
		// A weird little quirk of ImGui is that text starting with the percentage symbol can
		// have unintended side effects. To account for this, just change the names of any articles that begin
		// with percentages.
		if (line.substr(0, 1) == std::string("%")) {
			names.push_back("err");
		} else {
			names.push_back(line);
		}
		// Store the lowercase versions of names for querying (so we don't have to do this every time we query).
		std::string lowercaseName = std::string(line);
		std::transform(lowercaseName.begin(), lowercaseName.end(), lowercaseName.begin(),
			[](unsigned char c) { return std::tolower(c); });
		lowercaseNames.push_back(lowercaseName);
		numNames++;
	}

	std::ifstream linkFile("source/py/links.txt");
	if (!linkFile.is_open()) {
		std::cerr << "Error opening links file." << std::endl;
		return;
	}

	std::string linkLine;
	int i = -1;
	numLinks = 0;
	while (std::getline(linkFile, linkLine)) {
		if (numLinks >= MAX_NODES - 1) {
			AddToLog(std::string("Maximum links reached. Capped at " + std::to_string(numNames)).c_str());
			break;
		}
		// Parse the integer pairs on each row of the links.txt file.
		i++;
		numLinks++;
		int delPos = linkLine.find(",");
		std::string a = linkLine.substr(0, delPos);
		std::string b = linkLine.substr(delPos + 1);
		int idxA, idxB;
		try {
			idxA = std::stoi(a);
			idxB = std::stoi(b);
		} catch (const std::invalid_argument& e) {
			std::cerr << "Invalid argument: " << e.what() << std::endl;
			continue;
		} catch (const std::out_of_range& e) {
			std::cerr << "Out of range: " << e.what() << std::endl;
			continue;
		}
		// Assign relationary stuff in the linksFrom and links arrays.
		linksFrom[idxB] = idxA;
		links[i] = glm::vec2(idxA, idxB);
	}

	// Record the paths for each article.
	for (int i = 0; i < names.size(); i++) {
		std::vector<int> newPath = GetPath(i);
		paths.push_back(newPath);
	}

	// Record the time elapsed since generation begun.
	auto duration = std::chrono::duration_cast<std::chrono::seconds>(currentTime - startGeneratingTime);
	AddToLog(std::string("Time elapsed: " + std::to_string(duration.count()) + " seconds.").c_str());

	// Mark generation as complete.
	hasGeneratedMap = true;
	generatingMap = false;
	finishedWikipediaSearch = true;
}


// Begins generating a map by running the python script main.py, which populates the
// output.txt and links.txt text files. This method also fills the input.txt file 
// with the user's selected parameters.
void GenerateMap() {
	generatingMap = true;
	startGeneratingTime = std::chrono::high_resolution_clock::now();
	std::ofstream outputFile("source/py/input.txt");
	if (!outputFile.is_open()) {
		std::cerr << "Could not open input file" << std::endl;
		return;
	}

	outputFile.clear();
	outputFile << articleNameBuf << std::endl;
	outputFile << searchDepth << std::endl;
	outputFile << linksPerArticle << std::endl;
	outputFile.close();

	int result = std::system("py -m pip install requests");
	if (result == 0) {
		std::cout << "Requests module properly installed." << std::endl;
	} else {
		std::cerr << "Error executing requests module install. Error code: " << result << std::endl;
	}

	result = std::system("py source/py/main.py");

	if (result == 0) {
		std::cout << "Python script executed successfully." << std::endl;
	} else {
		std::cerr << "Error executing Python script. Error code: " << result << std::endl;
	}

	LoadData();
}


// Search callback method -- it's called when the text in the search bar changes.
int Search(ImGuiInputTextCallbackData* data) {
	if (data->BufTextLen == 0) {
		// The user has erased the contents of the search bar fully, so searching
		// should be set to false, and none of the articles should be highlighted.
		searching = false;
		getInfo = false;
		for (int i = 0; i < names.size(); i++) {
			highlighted[i] = 0;
		}
		updateHighlightBufferData = true;
		return 0;
	}
	searching = true;

	// If we are searching, we need to prepare to display info tooltips for
	// some articles. This means reading the positions of all the articles onto the CPU.
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, posBuffer);

	glm::vec2* positions = (glm::vec2*)glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, sizeof(glm::vec2) * names.size(), GL_MAP_READ_BIT);

	if (positions) {
		cpuNodeTranslations = positions;
		glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
		getInfo = true;
	}

	// Filter through the article names and decide whether they should be highlighted (2) or
	// not highlighted (1).
	std::string query = std::string(data->Buf);
	std::transform(query.begin(), query.end(), query.begin(),
		[](unsigned char c) { return std::tolower(c); });
	for (int i = 0; i < lowercaseNames.size(); i++) {
		std::string* n = &lowercaseNames.at(i);
		if (n->find(query) != std::string::npos) {
			highlighted[i] = 2;
		} else {
			highlighted[i] = 1;
		}
		updateHighlightBufferData = true;
	}
	return 0;
}


// Draws the main UI panel of the program.
void DrawManagerWindow() {
	ImGui::Begin("Map Manager");
	if (hasGeneratedMap) {
		// If the user has generated a map, they should see the settings they used, and should be given the option
		// to reset the map, or freeze/unfreeze the simulation.
		ImGui::Text("%s", std::string("Origin: " + std::string(articleNameBuf)).c_str());
		ImGui::Text("%s", std::string("Search depth: " + std::to_string(searchDepth)).c_str());
		ImGui::Text("%s", std::string("Limit of links per article: " + std::to_string(linksPerArticle)).c_str());
		ImGui::Text("%s", std::string("Number of articles: " + std::to_string(names.size())).c_str());
		ImGui::Separator();
		if (ImGui::Button("Reset map")) {
			std::cout << "resetting map" << std::endl;
			AddToLog(std::string("Reset map").c_str());
			names.clear();
			numLinks = 0;
			hasGeneratedMap = false;
		}
		if (!simFrozen) {
			if (ImGui::Button("Freeze simulation")) {
				AddToLog("Simulation frozen");
				simFrozen = true;
			}
		} else {
			if (ImGui::Button("Unfreeze simulation")) {
				AddToLog("Simulation unfrozen");
				simFrozen = false;
			}
		}
		
		// The user may enter text into this searchbox to search. See the Search callback method.
		ImGui::InputTextWithHint("##input2", "Search", searchBuf, IM_ARRAYSIZE(searchBuf), ImGuiInputTextFlags_CallbackEdit, Search);
	} else {
		if (!generatingMap) {
			// If the user has not yet generated a map, they should be prompted to do so and
			// may enter paramters.
			ImGui::TextWrapped("To generate a map, first enter an article name below.");
			ImGui::Separator();
			ImGui::SliderInt("Search depth", &searchDepth, 1, 5);
			ImGui::SliderInt("Links per article", &linksPerArticle, -1, 500);
			ImGui::InputTextWithHint("##input1", "Enter an article name", articleNameBuf, IM_ARRAYSIZE(articleNameBuf), ImGuiInputTextFlags_None);
			if (ImGui::Button("Generate map")) {
				std::cout << "adding map" << std::endl;
				AddToLog(std::string("Scanning Wikipedia from origin '" + std::string(articleNameBuf) + "'...").c_str());
				enqueueGenerateMap = true;
			}
		} else {
			// The map is being generated, so display a fun little loading bar
			// (it has no relation to the content being loaded or progress but it looks cool and signifies
			// that something is happening in the background.)
			int generatingTimeS = std::chrono::duration_cast<std::chrono::seconds>(currentTime - startGeneratingTime).count();
			int generatingTimeMs = std::chrono::duration_cast<std::chrono::microseconds>(currentTime - startGeneratingTime).count() / 1000;
			double progress = std::sin(((generatingTimeMs % 1000) / 1000.0) * (3.14159 / 2.0));
			ImGui::ProgressBar(progress, ImVec2(ImGui::GetWindowWidth() - 20, 15), "");
			ImGui::Separator();
			ImGui::Text("%s", std::string("Origin: " + std::string(articleNameBuf)).c_str());
			ImGui::Text("%s", std::string("Search depth: " + std::to_string(searchDepth)).c_str());
			ImGui::Text("%s", std::string("Limit of links per article: " + std::to_string(linksPerArticle)).c_str());
			ImGui::Text("%s", std::string("Time elapsed: " + std::to_string(generatingTimeS) + " seconds").c_str());
		}
	}
	
	ImGui::End();
}


// Draws a simple log window, containing whatever text is in logText.
void DrawLogWindow() {
	ImGui::Begin("Log");
	ImGui::TextUnformatted(logText.c_str());
	ImGui::SetScrollHereY(1.0f);
	ImGui::End();
}


// Draws a tooltip for less than maxTooltipsOnScreen of the nodes which are visible.
// If "searching" is true, the functionality will differ slightly, so as to provide more specific
// details rather than just the name of the article.
void DrawTooltip() {
	int displayed = 0;
	for (int i = 0; i < names.size(); i++) {
		// Ignore if we have already displayed the limit for the number of tooltips, or if we
		// are searching and this article doesn't match the query (highlighted[i] == 1).
		if (displayed > MAX_TOOLTIPS_VISIBLE) { break; }
		if (searching && highlighted[i] == 1) { continue; }

		glm::vec2 pos = (cpuNodeTranslations[i] - camPos) / zoom;
		ImVec2 guiPos = ImVec2(pos.x * (width / 2) + width / 2, -pos.y * (height / 2) + height / 2);
		bool visibleOnScreen;
		visibleOnScreen = guiPos.x > -50 && guiPos.x < width && guiPos.y > -50 && guiPos.y < height;

		if (visibleOnScreen) {
			displayed++;
			ImGui::SetNextWindowPos(guiPos);
			if (i >= names.size()) {
				std::cerr << "Index exceeds number of nodes" << std::endl;
				continue;
			}

			// Get the name of the article
			const char* str = names.at(i).c_str();

			if (searching) {
				// If searching, display more specific info, such as degree of separation
				int degreeSeparation = paths.at(i).size() - 1;
				std::string other = "\nLinks from: ";
				for (int j = 1; j < paths.at(i).size(); j++) {
					other.append("\n   ^ " + names.at(paths.at(i).at(j)));
				}
				if (degreeSeparation == 0) {
					other.append("none");
				}

				std::string from = std::string("Degree separation: " + std::to_string(degreeSeparation) + other);
				ImVec2 textSizeA = ImGui::CalcTextSize(from.c_str());
				ImVec2 textSizeB = ImGui::CalcTextSize(str);
				int textSize = std::max(textSizeA.x, textSizeB.x);
				ImGui::SetNextWindowSize(ImVec2(textSize + 20, textSizeB.y - 30));

				ImGui::Begin(std::to_string(i).c_str(), &isOpen, popupFlags);
				ImGui::TextColored(ImVec4(1, 1, 0, 1), str);
				ImGui::TextWrapped(from.c_str());
			} else {
				// Display only the article's title
				ImVec2 textSize = ImGui::CalcTextSize(str);
				ImGui::SetNextWindowSize(ImVec2(textSize.x + 20, textSize.y - 30));
				ImGui::Begin(std::to_string(i).c_str(), &isOpen, popupFlags);

				if (names.at(i).substr(0, 1) != std::string("%")) {
					ImGui::Text(str);
				}
			}
			ImGui::End();
		}
	}
}



// Called when the user scrolls. Used to zoom in and out.
void ScrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
	zoom -= (yoffset * scrollSpeed) * zoom;
}


// Called when the window is resized, and used to adjust the width and height
// of the viewport, as well as the speed at which the user pans the screen
void ResizeCallback(GLFWwindow* window, int x, int y) {
	width = x;
	height = y;
	panSpeedX = 2.0f / width;
	panSpeedY = 2.0f / height;
}


int main() {
	// Initialize OpenGL and create a window
	glfwInit();

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);

	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	GLFWwindow* window = glfwCreateWindow(width, height, "Wikipedia Mapper", NULL, NULL);

	if (window == NULL) {
		std::cout << "Failed to create GLFW window" << std::endl;
		glfwTerminate();
		return -1;
	}
	glfwMakeContextCurrent(window);

	gladLoadGL();

	// Set up shaders
	Shader shader = Shader("shaders/default.vert", "shaders/default.frag");
	Shader lineShader = Shader("shaders/line.vert", "shaders/line.frag");
	lineShader.Activate();
	glm::vec3 col = glm::vec3(0.1, 0.1, 0.1);
	glUniform3fv(glGetUniformLocation(lineShader.ID, "color"), 1, &col[0]);
	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
	glLineWidth(1);

	// Allocate memory for the big arrays, but in a separate thread to prevent freezing
	allocateMemory = std::thread(AllocateMemory);

	// Create the position buffer used to store the articles' positions in the compute shader
	glGenBuffers(1, &posBuffer);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, posBuffer);

	// Create the compute shader
	GLuint computeShader = Shader::CreateComputeShader("shaders/particles.comp");
	GLuint program = Shader::CreateShaderProgram(computeShader);

	// Bind vertex arrays and 
	GLuint VAO, VBO, highlightVBO;

	glGenVertexArrays(1, &VAO);
	glGenBuffers(1, &VBO);
	glGenBuffers(1, &highlightVBO);

	glBindVertexArray(VAO);

	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

	// Bind vert position array and instance offset array
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);
	glBindBuffer(GL_ARRAY_BUFFER, posBuffer);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glVertexAttribDivisor(1, 1);

	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

	glGenBuffers(1, &lineBuffer);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, lineBuffer);

	// Bind line translation array
	glEnableVertexAttribArray(2);
	glBindBuffer(GL_ARRAY_BUFFER, lineBuffer);
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glVertexAttribDivisor(2, 0); // Divisor is zero so that each value in the array
								 // is specific to each vertex rather than each instance

	glBindBuffer(GL_ARRAY_BUFFER, 0);

	// Bind highlight array
	glBindBuffer(GL_ARRAY_BUFFER, highlightVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(int) * MAX_NODES, nullptr, GL_DYNAMIC_DRAW);
	glEnableVertexAttribArray(3);
	glVertexAttribIPointer(3, 1, GL_INT, 1 * sizeof(int), (void*)0);
	glVertexAttribDivisor(3, 1);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	// Unbind buffers just to be safe
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
	glBindVertexArray(0);

	// Set up Imgui
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	ImGui::StyleColorsDark();
	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init("#version 430");

	// Set up some callbacks
	glfwSetScrollCallback(window, ScrollCallback);
	glfwSetWindowSizeCallback(window, ResizeCallback);

	GLuint workGroupSize = 64;

	GLuint scaleUniform = glGetUniformLocation(shader.ID, "zoom"); 
	GLuint camUniform = glGetUniformLocation(shader.ID, "camOffset"); 
	GLuint highlightUniform = glGetUniformLocation(shader.ID, "highlighted");
	GLuint scaleUniformLine = glGetUniformLocation(lineShader.ID, "zoom");
	GLuint camUniformLine = glGetUniformLocation(lineShader.ID, "camOffset");
	GLuint searchingUniformLine = glGetUniformLocation(lineShader.ID, "searching");

	// Main loop
	while (!glfwWindowShouldClose(window)) {
		// Prepare info text for info window
		noNodes = "No. Nodes: " + std::to_string(names.size());
		noConnections = "No. Links: " + std::to_string(numLinks);

		// Get current time
		currentTime = std::chrono::high_resolution_clock::now();

		// Prepare window for rendering
		glClearColor(0.1f, 0.1f, 0.13f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);
		glViewport(0, 0, width, height);

		// If the map is to be generated, start a new thread
		if (enqueueGenerateMap) {
			generateThread = std::thread(GenerateMap);
			enqueueGenerateMap = false;
		}

		// If the scan failed, join the still running thread
		if (failedWikipediaSearch) {
			generateThread.join();
			failedWikipediaSearch = false;
		}

		// If the scan was successful, join the thread and add the nodes
		if (finishedWikipediaSearch) {
			generateThread.join();
			FillNodes();
			finishedWikipediaSearch = false;
		}

		// If the user is searching, show tooltips at all times
		if (searching) {
			getInfo = true;
		}

		// Only if tooltips are not being shown and the map is fully generated,
		// dispatch the compute shader.
		if (!getInfo && hasGeneratedMap && !generatingMap && !simFrozen) {
			glUseProgram(program);

			if (numWorkGroups == 0) {
				numWorkGroups = (names.size() + workGroupSize - 1) / workGroupSize;
			}

			glDispatchCompute(numWorkGroups, 1, 1);
			glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
		}

		// Start Imgui this frame
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		glfwPollEvents();
		// If the keyboard is not being used by the UI and the I key is pressed...
		if (!ImGui::GetIO().WantCaptureKeyboard) {
			if (glfwGetKey(window, GLFW_KEY_I) == GLFW_PRESS && !generatingMap) {
				if (!getInfo) {
					// Map the position buffer to CPU memory so specifically the nodes visible on screen
					// can be displayed for optimization
					glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
					glBindBuffer(GL_SHADER_STORAGE_BUFFER, posBuffer);

					glm::vec2* positions = (glm::vec2*)glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, sizeof(glm::vec2) * names.size(), GL_MAP_READ_BIT);

					if (positions) {
						cpuNodeTranslations = positions;
						glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
						getInfo = true;
					}
					else {
						std::cerr << "Failed to map buffer." << std::endl;
					}
				}
			}
			if (glfwGetKey(window, GLFW_KEY_I) == GLFW_RELEASE && !searching) {
				getInfo = false;
			}
		}

		// Get mouse cursor position
		glfwGetCursorPos(window, &relativeCursorX, &relativeCursorY);

		// Unless Imgui is using the mouse for something, allow for panning and zooming
		if (!ImGui::GetIO().WantCaptureMouse && hasGeneratedMap) {
			if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
				panning = true;
			}
			if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_RELEASE) {
				panning = false;
			}
			if (panning) {
				camPos += zoom * glm::vec2(panSpeedX * (lastRelativeCursorX - relativeCursorX), panSpeedY * (- lastRelativeCursorY + relativeCursorY));
			}
		}

		// If the map is generated, draw the nodes and lines
		if (hasGeneratedMap && !generatingMap) {
			glBindVertexArray(VAO);
			lineShader.Activate();
			// Update uniforms
			glUniform1f(scaleUniformLine, zoom);
			glUniform2f(camUniformLine, camPos.x, camPos.y);
			glUniform1i(searchingUniformLine, searching);
			glDrawArraysInstanced(GL_LINES, 0, numLinks * 2, 1);
			shader.Activate();
			if (updateHighlightBufferData) {
				// If the user just changed their search query, inform the GPU of what
				// nodes are now highlighted

				glBindBuffer(GL_ARRAY_BUFFER, highlightVBO);
				glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(int) * MAX_NODES, highlighted);
				glBindBuffer(GL_ARRAY_BUFFER, 0);

				updateHighlightBufferData = false;
			}
			glUniform1f(scaleUniform, zoom); 
			glUniform2f(camUniform, camPos.x, camPos.y); 
			glDrawArraysInstanced(GL_TRIANGLES, 0, 6, names.size());
			// Unbind vertex array once done
			glBindVertexArray(0);
		}

		CalculateFrameRate();

		// Draw the necessary Imgui windows
		if (getInfo && !generatingMap) {
			DrawTooltip();
		}
		if (memoryAllocated) {
			DrawManagerWindow();
		}
		DrawStatsWindow();
		DrawLogWindow();

		// Render Imgui
		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

		glfwSwapBuffers(window);

		// Update for deltas
		lastTime = currentTime;
		lastRelativeCursorX = relativeCursorX;
		lastRelativeCursorY = relativeCursorY;
	}

	// End program, join any running threads, and free allocated memory 
	if (generateThread.joinable()) { generateThread.join(); }
	if (allocateMemory.joinable()) { allocateMemory.join(); }
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
	glDeleteVertexArrays(1, &VAO);
	glDeleteVertexArrays(1, &VBO);
	glDeleteBuffers(1, &highlightVBO);
	free(nodeTranslations);
	free(lineTranslations);
	free(cpuNodeTranslations);
	free(highlighted);
	free(linksFrom);
	delete &names;
	delete &lowercaseNames;
	glDeleteBuffers(1, &posBuffer);
	shader.Delete();
	glfwDestroyWindow(window);
	glfwTerminate();
	return 0;
}



