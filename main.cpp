#include "grobid_utils.h"
#include <iostream>

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <pdf_path>\n";
        return 1;
    }

    std::string pdf_path = argv[1];

    extract_pdf_references_with_grobid(pdf_path);

    std::cout << "Done.\n";
    return 0;
}
