#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <regex>
#include <iomanip>

enum TokenType {
    LABEL,
    INSTRUCTION,
    DATA_BYTES,
    DATA_WORDS,
    DIRECTIVE,
    CONSTANT_DECL,
    COMMENT,
    UNKNOWN
};

struct Token {
    TokenType type;
    std::string value;
    std::string operand;
    std::string comment;
    int lineNumber;
    std::vector<std::string> dataValues;
};

class AssemblyToJsonConverter {
private:
    std::vector<Token> tokens;
    std::map<std::string, std::string> constants;
    
    bool isInstruction(const std::string& word) {
        static const std::vector<std::string> instructions = {
            "lda", "ldx", "ldy", "sta", "stx", "sty",
            "tax", "tay", "txa", "tya", "tsx", "txs",
            "pha", "php", "pla", "plp",
            "and", "eor", "ora", "bit",
            "adc", "sbc", "cmp", "cpx", "cpy",
            "inc", "inx", "iny", "dec", "dex", "dey",
            "asl", "lsr", "rol", "ror",
            "jmp", "jsr", "rts",
            "bcc", "bcs", "beq", "bmi", "bne", "bpl", "bvc", "bvs",
            "clc", "cld", "cli", "clv", "sec", "sed", "sei",
            "brk", "nop", "rti"
        };
        
        for (const auto& inst : instructions) {
            if (word == inst) return true;
        }
        return false;
    }
    
    std::string trim(const std::string& str) {
        size_t start = str.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) return "";
        size_t end = str.find_last_not_of(" \t\r\n");
        return str.substr(start, end - start + 1);
    }
    
    std::vector<std::string> split(const std::string& str, char delimiter) {
        std::vector<std::string> tokens;
        std::string token;
        std::istringstream tokenStream(str);
        while (std::getline(tokenStream, token, delimiter)) {
            tokens.push_back(trim(token));
        }
        return tokens;
    }
    
    std::string extractComment(const std::string& line) {
        size_t commentPos = line.find(';');
        if (commentPos != std::string::npos) {
            return trim(line.substr(commentPos + 1));
        }
        return "";
    }
    
    std::string removeComment(const std::string& line) {
        size_t commentPos = line.find(';');
        if (commentPos != std::string::npos) {
            return trim(line.substr(0, commentPos));
        }
        return trim(line);
    }
    
    TokenType classifyLine(const std::string& line, Token& token) {
        std::string cleanLine = removeComment(line);
        token.comment = extractComment(line);
        
        if (cleanLine.empty()) {
            return COMMENT;
        }
        
        // Check for label (ends with :)
        if (cleanLine.back() == ':') {
            token.value = cleanLine.substr(0, cleanLine.length() - 1);
            return LABEL;
        }
        
        // Check for constant declaration (contains =)
        size_t equalPos = cleanLine.find('=');
        if (equalPos != std::string::npos) {
            token.value = trim(cleanLine.substr(0, equalPos));
            token.operand = trim(cleanLine.substr(equalPos + 1));
            constants[token.value] = token.operand;
            return CONSTANT_DECL;
        }
        
        // Check for data directives
        if (cleanLine.substr(0, 3) == ".db") {
            token.value = ".db";
            std::string dataStr = trim(cleanLine.substr(3));
            token.dataValues = split(dataStr, ',');
            return DATA_BYTES;
        }
        
        if (cleanLine.substr(0, 3) == ".dw") {
            token.value = ".dw";
            std::string dataStr = trim(cleanLine.substr(3));
            token.dataValues = split(dataStr, ',');
            return DATA_WORDS;
        }
        
        // Check for other directives (start with .)
        if (cleanLine[0] == '.') {
            std::istringstream iss(cleanLine);
            iss >> token.value >> token.operand;
            return DIRECTIVE;
        }
        
        // Check for instructions
        std::istringstream iss(cleanLine);
        std::string firstWord;
        iss >> firstWord;
        
        if (isInstruction(firstWord)) {
            token.value = firstWord;
            std::string rest;
            std::getline(iss, rest);
            token.operand = trim(rest);
            return INSTRUCTION;
        }
        
        return UNKNOWN;
    }
    
    std::string escapeJson(const std::string& str) {
        std::string escaped;
        for (char c : str) {
            switch (c) {
                case '"': escaped += "\\\""; break;
                case '\\': escaped += "\\\\"; break;
                case '\b': escaped += "\\b"; break;
                case '\f': escaped += "\\f"; break;
                case '\n': escaped += "\\n"; break;
                case '\r': escaped += "\\r"; break;
                case '\t': escaped += "\\t"; break;
                default: escaped += c; break;
            }
        }
        return escaped;
    }
    
public:
    void parseFile(const std::string& filename) {
        std::ifstream file(filename);
        if (!file.is_open()) {
            throw std::runtime_error("Cannot open file: " + filename);
        }
        
        std::string line;
        int lineNumber = 1;
        
        while (std::getline(file, line)) {
            Token token;
            token.lineNumber = lineNumber;
            token.type = classifyLine(line, token);
            
            if (token.type != COMMENT || !token.comment.empty()) {
                tokens.push_back(token);
            }
            
            lineNumber++;
        }
    }
    
    std::string generateJson() {
        std::ostringstream json;
        json << "{\n";
        json << "  \"assembly_program\": {\n";
        json << "    \"metadata\": {\n";
        json << "      \"total_lines\": " << tokens.size() << ",\n";
        json << "      \"processor\": \"6502\"\n";
        json << "    },\n";
        
        // Constants section
        json << "    \"constants\": [\n";
        bool firstConstant = true;
        for (const auto& token : tokens) {
            if (token.type == CONSTANT_DECL) {
                if (!firstConstant) json << ",\n";
                json << "      {\n";
                json << "        \"name\": \"" << escapeJson(token.value) << "\",\n";
                json << "        \"value\": \"" << escapeJson(token.operand) << "\",\n";
                json << "        \"line\": " << token.lineNumber;
                if (!token.comment.empty()) {
                    json << ",\n        \"comment\": \"" << escapeJson(token.comment) << "\"";
                }
                json << "\n      }";
                firstConstant = false;
            }
        }
        json << "\n    ],\n";
        
        // Labels section
        json << "    \"labels\": [\n";
        bool firstLabel = true;
        for (const auto& token : tokens) {
            if (token.type == LABEL) {
                if (!firstLabel) json << ",\n";
                json << "      {\n";
                json << "        \"name\": \"" << escapeJson(token.value) << "\",\n";
                json << "        \"line\": " << token.lineNumber;
                if (!token.comment.empty()) {
                    json << ",\n        \"comment\": \"" << escapeJson(token.comment) << "\"";
                }
                json << "\n      }";
                firstLabel = false;
            }
        }
        json << "\n    ],\n";
        
        // Instructions section
        json << "    \"instructions\": [\n";
        bool firstInstruction = true;
        for (const auto& token : tokens) {
            if (token.type == INSTRUCTION) {
                if (!firstInstruction) json << ",\n";
                json << "      {\n";
                json << "        \"mnemonic\": \"" << escapeJson(token.value) << "\",\n";
                json << "        \"operand\": \"" << escapeJson(token.operand) << "\",\n";
                json << "        \"line\": " << token.lineNumber;
                if (!token.comment.empty()) {
                    json << ",\n        \"comment\": \"" << escapeJson(token.comment) << "\"";
                }
                json << "\n      }";
                firstInstruction = false;
            }
        }
        json << "\n    ],\n";
        
        // Data section
        json << "    \"data\": [\n";
        bool firstData = true;
        for (const auto& token : tokens) {
            if (token.type == DATA_BYTES || token.type == DATA_WORDS) {
                if (!firstData) json << ",\n";
                json << "      {\n";
                json << "        \"directive\": \"" << escapeJson(token.value) << "\",\n";
                json << "        \"type\": \"" << (token.type == DATA_BYTES ? "bytes" : "words") << "\",\n";
                json << "        \"values\": [";
                for (size_t i = 0; i < token.dataValues.size(); ++i) {
                    if (i > 0) json << ", ";
                    json << "\"" << escapeJson(token.dataValues[i]) << "\"";
                }
                json << "],\n";
                json << "        \"line\": " << token.lineNumber;
                if (!token.comment.empty()) {
                    json << ",\n        \"comment\": \"" << escapeJson(token.comment) << "\"";
                }
                json << "\n      }";
                firstData = false;
            }
        }
        json << "\n    ],\n";
        
        // Directives section
        json << "    \"directives\": [\n";
        bool firstDirective = true;
        for (const auto& token : tokens) {
            if (token.type == DIRECTIVE) {
                if (!firstDirective) json << ",\n";
                json << "      {\n";
                json << "        \"name\": \"" << escapeJson(token.value) << "\",\n";
                json << "        \"operand\": \"" << escapeJson(token.operand) << "\",\n";
                json << "        \"line\": " << token.lineNumber;
                if (!token.comment.empty()) {
                    json << ",\n        \"comment\": \"" << escapeJson(token.comment) << "\"";
                }
                json << "\n      }";
                firstDirective = false;
            }
        }
        json << "\n    ],\n";
        
        // Sequential program flow
        json << "    \"program_flow\": [\n";
        bool firstFlow = true;
        for (const auto& token : tokens) {
            if (token.type != COMMENT) {
                if (!firstFlow) json << ",\n";
                json << "      {\n";
                json << "        \"line\": " << token.lineNumber << ",\n";
                json << "        \"type\": \"";
                
                switch (token.type) {
                    case LABEL: json << "label"; break;
                    case INSTRUCTION: json << "instruction"; break;
                    case DATA_BYTES: 
                    case DATA_WORDS: json << "data"; break;
                    case DIRECTIVE: json << "directive"; break;
                    case CONSTANT_DECL: json << "constant"; break;
                    default: json << "unknown"; break;
                }
                
                json << "\",\n";
                json << "        \"content\": \"" << escapeJson(token.value);
                if (!token.operand.empty()) {
                    json << " " << escapeJson(token.operand);
                }
                json << "\"";
                
                if (!token.comment.empty()) {
                    json << ",\n        \"comment\": \"" << escapeJson(token.comment) << "\"";
                }
                
                json << "\n      }";
                firstFlow = false;
            }
        }
        json << "\n    ]\n";
        
        json << "  }\n";
        json << "}\n";
        
        return json.str();
    }
};

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <input.asm> <output.json>" << std::endl;
        return 1;
    }
    
    try {
        AssemblyToJsonConverter converter;
        converter.parseFile(argv[1]);
        
        std::string jsonOutput = converter.generateJson();
        
        std::ofstream outputFile(argv[2]);
        if (!outputFile.is_open()) {
            throw std::runtime_error("Cannot create output file: " + std::string(argv[2]));
        }
        
        outputFile << jsonOutput;
        outputFile.close();
        
        std::cout << "Successfully converted " << argv[1] << " to " << argv[2] << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
