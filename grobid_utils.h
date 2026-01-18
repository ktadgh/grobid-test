#pragma once

#include <string>
#include <vector>
#include <filesystem>

namespace fs = std::filesystem;

std::string url_encode(const std::string& value);


std::string read_file_binary(const fs::path& path);

bool ensure_grobid_running();

std::string query_crossref_for_doi(const std::string& title);

std::string normalize_arxiv_from_text(const std::string& text);

std::vector<std::string> extract_biblstruct_text(const std::string& tei_xml);

void extract_pdf_references_with_grobid(const std::string& pdf_path);

std::string query_arxiv_for_doi(const std::string& title);
