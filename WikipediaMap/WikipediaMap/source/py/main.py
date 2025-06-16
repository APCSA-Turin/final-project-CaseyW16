import requests
import time
import threading
import os

total_requests = 0
use_multithreading = True
links_list = []
articles_visited = []

# Gets the local directory path
__location__ = os.path.realpath(
    os.path.join(os.getcwd(), os.path.dirname(__file__)))


# The main method -- queries and writes
def query(start, depth, num_links):
    global total_requests
    total_requests = 0
    start_time = time.time()
    all = get_nodes([], start, depth, num_links, True)
    end_time = time.time()

    if not all:
        # If no articles are found, note this in the output file with a special string
        with open(os.path.join(__location__, 'output.txt'), "w", encoding="utf-8") as file:
            file.write("NOARTICLESFOUND!\n")
        return

    print ("Requests finished")
    print(f"The search took {end_time - start_time} seconds.")

    all = list(set(all))  # Remove duplicates
    all.sort()

    unique_links = []
    for a in links_list:
         if a[0] == a[1]: continue
         if [a[0], a[1]] not in unique_links and [a[1], a[0]] not in unique_links:
             unique_links.append([a[0], a[1]])
  
    print ("Sorting finished and duplicates removed")

    with open(os.path.join(__location__, 'output.txt'), "w", encoding="utf-8") as file:
        file.write("\n".join(all))

    with open(os.path.join(__location__, 'links.txt'), "w", encoding="utf-8") as file:
        for link in unique_links:
            for a in link:
                file.write(str(all.index(a)) + ",")
            file.write("\n")


    return [all, unique_links]


# A recursive method that scans branches of Wikipedia articles, creating a new thread
# for each branch of only the first article.
def get_nodes(all, start, depth, num_links, is_start):
    if depth == 0:
        return []
    page_lim = num_links
    if is_start:
        page_lim = -1
        start = fix_capitalization_and_check_valid(start)
        if start is None:
            print("Article not found")
            return []
        all = [start]
    new = links_from_wikipedia_page(start, page_lim)
    if not new:
        return []
    all.extend(new)
    threads = []
    if is_start:
        print("Starting branches: ", len(new))
    for node in new:
        if not node in articles_visited:
            articles_visited.append(node)
        else:
            continue
        links_list.append([start, node])
        if not is_start:
            get_nodes(all, node, depth - 1, num_links, False)
        else:
            if use_multithreading:
                t = threading.Thread(target=get_nodes, args=(all, node, depth - 1, num_links, False))
                t.start()
                threads.append(t)
            else:
                get_nodes(all, node, depth - 1, num_links, False)
    for t in threads:
        t.join()
    return all


# Gets the JSON data from the URL
def get_wikipedia_links_from_json(url):
    try:
        response = requests.get(url)
        response.raise_for_status()
        data = response.json()
        return data
    except requests.exceptions.RequestException as e:
        print(f"Error during request: {e}")
        return None


# Extracts only the links from the data
def extract_links(data):
    links = []
    if data and 'query' in data and 'pages' in data['query']:
        for page in data['query']['pages'].values():
            if 'links' in page:
                for link in page['links']:
                    title = link['title']
                    if (title.startswith('Wikipedia:') or title.startswith('Template:') or title.startswith('Category:')
                            or title.startswith(".") or title.startswith('Talk:') or title.startswith('Help:')
                            or title.startswith("Template talk:") or title.startswith("Portal:")
                            or title.startswith("File:")) or title.startswith("H:"): continue
                    links.append(link['title'])
    return links


# If the capitalization is slightly off, Wikipedia will have a redirect.
# This method checks for any redirects and updates the origin page name accordingly.
def fix_capitalization_and_check_valid(page_name):
    url = f"https://en.wikipedia.org/w/api.php"
    params = {
        "action": "query",
        "format": "json",
        "prop": "links",
        "titles": page_name,
        "pllimit": "1"
    }

    correct_title = page_name

    url_with_params = f"{url}?{requests.compat.urlencode(params)}"

    data = get_wikipedia_links_from_json(url_with_params)
    all_links = extract_links(data)

    if data is None:
        return None
    if 'query' not in data:
        return None
    if '-1' in data['query']['pages']:
        return None

    if "normalized" in data['query']:
        correct_title = data['query']['normalized'][0]['to']

    return correct_title


# Gets a list of links from the given page name and a specified limit
def links_from_wikipedia_page(page_name, num_links):
    global total_requests

    url = f"https://en.wikipedia.org/w/api.php"
    params = {
        "action": "query",
        "format": "json",
        "prop": "links",
        "titles": page_name,
        "pllimit": str(num_links)
    }

    if num_links == -1:
        params['pllimit'] = "max"

    url_with_params = f"{url}?{requests.compat.urlencode(params)}"
    data = get_wikipedia_links_from_json(url_with_params)
    all_links = extract_links(data)
    total_requests += 1

    if data is None:
        return []
    if 'query' not in data:
        return []

    while "continue" in data and num_links == -1:
        plcontinue = data['continue']['plcontinue']
        params['plcontinue'] = plcontinue
        new_url = f"{url}?{requests.compat.urlencode(params)}"
        data = get_wikipedia_links_from_json(new_url)
        total_requests += 1
        all_links.extend(extract_links(data))

    return all_links


if __name__ == "__main__":
    with open(os.path.join(__location__, 'input.txt'), "r") as file:
        lines = file.readlines()
        query(lines[0].strip(), int(lines[1].strip()), int(lines[2].strip()))