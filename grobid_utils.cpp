#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <regex>
#include <unordered_map>
#include "grobid_utils.h"

#include <httplib.h>
#include <tinyxml2.h>
#include <nlohmann/json.hpp>

#include <QProcess>
#include <QThread>

namespace fs = std::filesystem;
using json = nlohmann::json;

/* ============================================================
   Globals
   ============================================================ */

static QProcess* g_grobid_process = nullptr; // static here ensures it only exists in this file

/* ============================================================
   Utilities
   ============================================================ */

std::string url_encode(const std::string& value) { //compiler will enforce that it isn't changed
    std::ostringstream escaped; //just for simple string concat
    escaped << std::hex << std::uppercase;

    for (unsigned char c : value) {
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            escaped << c;
        } else if (c == ' ') {
            escaped << "%20";
        } else {
            escaped << '%' << std::setw(2) << std::setfill('0') << int(c);
        }
    }
    return escaped.str();
}

std::string read_file_binary(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

// running grobid
bool ensure_grobid_running() {
    httplib::Client cli("http://localhost:8070");
    cli.set_read_timeout(2, 0);

    if (auto res = cli.Get("/api/isalive"); res && res->status == 200) {
        return true;
    }

    if (!g_grobid_process) {
        g_grobid_process = new QProcess();
        QStringList args;
        args << "-jar"
             << "/Users/tadghk/grobid/grobid-core/build/libs/grobid-core-0.8.3-SNAPSHOT.jar"
             << "server";

        g_grobid_process->start("java", args);
        if (!g_grobid_process->waitForStarted(5000)) {
            std::cerr << "Failed to start Grobid\n";
            return false;
        }
    }

    for (int i = 0; i < 20; ++i) {
        if (auto res = cli.Get("/api/isalive"); res && res->status == 200) {
            return true;
        }
        QThread::sleep(1);
    }

    std::cerr << "Grobid did not respond in time\n";
    return false;
}

// querying crossref
std::string query_crossref_for_doi(const std::string& title) {
    if (title.empty()) return "";

    httplib::Client cli("https://api.crossref.org");
    cli.set_read_timeout(10, 0);

    std::string url = "/works?query.title=" + url_encode(title) +
                      "&rows=1&mailto=you@example.com";

    auto res = cli.Get(url.c_str());
    if (!res || res->status != 200) return "";

    try {
        auto j = json::parse(res->body);
        auto& items = j["message"]["items"];
        if (!items.empty() && items[0].contains("DOI")) {
            return items[0]["DOI"].get<std::string>();
        }
    } catch (...) {
        return "";
    }

    return "";
}

// getting doi from text
std::string normalize_arxiv_from_text(const std::string& text) {
    std::regex re(R"(arXiv:(\d{4}\.\d{4,5}))");
    std::smatch m;
    if (std::regex_search(text, m, re)) {
        return "10.48550/arXiv." + m[1].str();
    }
    return "";
}


// getting bibliography from text
std::vector<std::string> extract_biblstruct_text(const std::string& tei_xml) {
    std::vector<std::string> results;

    tinyxml2::XMLDocument doc;
    if (doc.Parse(tei_xml.c_str()) != tinyxml2::XML_SUCCESS) {
        return results;
    }

    auto* root = doc.RootElement();
    if (!root) return results;

    for (auto* bibl = root->FirstChildElement("text")
                              ->FirstChildElement("back")
                              ->FirstChildElement("div")
                              ->FirstChildElement("listBibl")
                              ->FirstChildElement("biblStruct");
         bibl;
         bibl = bibl->NextSiblingElement("biblStruct")) {

        std::ostringstream text;

        for (auto* el = bibl->FirstChildElement(); el; el = el->NextSiblingElement()) {
            if (el->GetText()) {
                text << el->GetText() << " ";
            }
        }

        std::string s = text.str();
        if (!s.empty()) results.push_back(s);
    }

    return results;
}


// get references from a pdf
void extract_pdf_references_with_grobid(const std::string& pdf_path) {
    if (!fs::exists(pdf_path)) {
        std::cerr << "PDF does not exist\n";
        return;
    }

    if (!ensure_grobid_running()) {
        return;
    }

    std::string pdf_data = read_file_binary(pdf_path);

    httplib::Client cli("http://localhost:8070");
    cli.set_read_timeout(60, 0);


    // create a vector of upload form data items
    httplib::UploadFormDataItems items = {
        {
            "input",                                 // field name
            pdf_data,                                // file content
            fs::path(pdf_path).filename().string(),  // filename
            "application/pdf"                        // content type
        }
    };

    // post the multipart/form-data
    auto res = cli.Post("/api/processReferences", httplib::Headers{}, items);


    if (!res || res->status != 200) {
        std::cerr << "Grobid failed\n";
        return;
    }

    auto refs = extract_biblstruct_text(res->body);

    fs::path out_path = fs::path(pdf_path).replace_extension(".md");
    std::ofstream out(out_path);

    for (auto& ref : refs) {
        std::string doi = normalize_arxiv_from_text(ref);

        if (doi.empty()) {
            doi = query_arxiv_for_doi(ref);
        }

        if (doi.empty()) {
            doi = query_crossref_for_doi(ref);
        }

        out << "- " << ref;
        if (!doi.empty()) out << "  \n  DOI: " << doi;
        out << "\n";
    }

    std::cout << "References written to " << out_path << "\n";
}

// searching arXiv
std::string query_arxiv_for_doi(const std::string& title) {
    if (title.empty()) return "";

    httplib::Client cli("http://export.arxiv.org");
    cli.set_read_timeout(10, 0);

    std::string url =
        "/api/query?search_query=ti:" + url_encode(title) +
        "&start=0&max_results=1";

    auto res = cli.Get(url.c_str());
    if (!res || res->status != 200) return "";

    tinyxml2::XMLDocument doc;
    if (doc.Parse(res->body.c_str()) != tinyxml2::XML_SUCCESS) {
        return "";
    }

    auto* feed = doc.FirstChildElement("feed");
    if (!feed) return "";

    auto* entry = feed->FirstChildElement("entry");
    if (!entry) return "";

    auto* id_el = entry->FirstChildElement("id");
    if (!id_el || !id_el->GetText()) return "";

    std::string id_url = id_el->GetText();
    // example: http://arxiv.org/abs/2301.01234v2

    std::regex re(R"(arxiv\.org/abs/(\d{4}\.\d{4,5}))");
    std::smatch m;
    if (!std::regex_search(id_url, m, re)) return "";

    return "10.48550/arXiv." + m[1].str();
}
