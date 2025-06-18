#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <regex>
#include <cctype>

struct ProgramLine {
    int lineNumber;
    std::string type;
    std::string content;
    std::string comment;
    
    // For reconstructing original format
    std::string name;
    std::string value;
    std::string operand;
    std::string mnemonic;
    std::string directive;
    std::vector<std::string> values;
};

class JsonToAssemblyConverter {
private:
    std::vector<ProgramLine> programFlow;
    std::map<int, ProgramLine> lineMap;
    
    std::string unescapeJson(const std::string& str) {
        std::string unescaped;
        for (size_t i = 0; i < str.length(); ++i) {
            if (str[i] == '\\' && i + 1 < str.length()) {
                switch (str[i + 1]) {
                    case '"': unescaped += '"'; i++; break;
                    case '\\': unescaped += '\\'; i++; break;
                    case 'b': unescaped += '\b'; i++; break;
                    case 'f': unescaped += '\f'; i++; break;
                    case 'n': unescaped += '\n'; i++; break;
                    case 'r': unescaped += '\r'; i++; break;
                    case 't': unescaped += '\t'; i++; break;
                    default: unescaped += str[i]; break;
                }
            } else {
                unescaped += str[i];
            }
        }
        return unescaped;
    }
    
    std::string extractStringValue(const std::string& json, const std::string& key) {
        std::string searchKey = "\"" + key + "\"";
        size_t keyPos = json.find(searchKey);
        if (keyPos == std::string::npos) return "";
        
        size_t colonPos = json.find(":", keyPos);
        if (colonPos == std::string::npos) return "";
        
        size_t startQuote = json.find("\"", colonPos);
        if (startQuote == std::string::npos) return "";
        
        size_t endQuote = startQuote + 1;
        while (endQuote < json.length()) {
            if (json[endQuote] == '"' && json[endQuote - 1] != '\\') {
                break;
            }
            endQuote++;
        }
        
        if (endQuote >= json.length()) return "";
        
        return unescapeJson(json.substr(startQuote + 1, endQuote - startQuote - 1));
    }
    
    int extractIntValue(const std::string& json, const std::string& key) {
        std::string searchKey = "\"" + key + "\"";
        size_t keyPos = json.find(searchKey);
        if (keyPos == std::string::npos) return -1;
        
        size_t colonPos = json.find(":", keyPos);
        if (colonPos == std::string::npos) return -1;
        
        size_t numStart = colonPos + 1;
        while (numStart < json.length() && (json[numStart] == ' ' || json[numStart] == '\t')) {
            numStart++;
        }
        
        size_t numEnd = numStart;
        while (numEnd < json.length() && (std::isdigit(json[numEnd]) || json[numEnd] == '-')) {
            numEnd++;
        }
        
        if (numEnd > numStart) {
            return std::stoi(json.substr(numStart, numEnd - numStart));
        }
        
        return -1;
    }
    
    std::vector<std::string> extractArrayValues(const std::string& json, const std::string& key) {
        std::vector<std::string> values;
        std::string searchKey = "\"" + key + "\"";
        size_t keyPos = json.find(searchKey);
        if (keyPos == std::string::npos) return values;
        
        size_t colonPos = json.find(":", keyPos);
        if (colonPos == std::string::npos) return values;
        
        size_t arrayStart = json.find("[", colonPos);
        if (arrayStart == std::string::npos) return values;
        
        size_t arrayEnd = json.find("]", arrayStart);
        if (arrayEnd == std::string::npos) return values;
        
        std::string arrayContent = json.substr(arrayStart + 1, arrayEnd - arrayStart - 1);
        
        // Parse array elements - handle both quoted and unquoted values
        size_t pos = 0;
        while (pos < arrayContent.length()) {
            // Skip whitespace and commas
            while (pos < arrayContent.length() && (arrayContent[pos] == ' ' || 
                   arrayContent[pos] == '\t' || arrayContent[pos] == ',' || 
                   arrayContent[pos] == '\n' || arrayContent[pos] == '\r')) {
                pos++;
            }
            
            if (pos >= arrayContent.length()) break;
            
            if (arrayContent[pos] == '"') {
                // Handle quoted string
                size_t startQuote = pos;
                size_t endQuote = startQuote + 1;
                while (endQuote < arrayContent.length()) {
                    if (arrayContent[endQuote] == '"' && arrayContent[endQuote - 1] != '\\') {
                        break;
                    }
                    endQuote++;
                }
                
                if (endQuote < arrayContent.length()) {
                    values.push_back(unescapeJson(arrayContent.substr(startQuote + 1, endQuote - startQuote - 1)));
                    pos = endQuote + 1;
                }
            } else {
                // Handle unquoted value (should not occur in proper JSON, but handle it)
                size_t valueStart = pos;
                while (pos < arrayContent.length() && arrayContent[pos] != ',' && 
                       arrayContent[pos] != ']' && arrayContent[pos] != '\n') {
                    pos++;
                }
                std::string value = arrayContent.substr(valueStart, pos - valueStart);
                // Trim the value
                value = std::regex_replace(value, std::regex("^\\s+|\\s+$"), "");
                if (!value.empty()) {
                    values.push_back(value);
                }
            }
        }
        
        return values;
    }
    
    void parseJsonSection(const std::string& json, const std::string& sectionName) {
        std::string searchPattern = "\"" + sectionName + "\"";
        size_t sectionStart = json.find(searchPattern);
        if (sectionStart == std::string::npos) return;
        
        size_t arrayStart = json.find("[", sectionStart);
        if (arrayStart == std::string::npos) return;
        
        int bracketCount = 0;
        size_t pos = arrayStart;
        
        // Find the matching closing bracket
        while (pos < json.length()) {
            if (json[pos] == '[') bracketCount++;
            else if (json[pos] == ']') bracketCount--;
            
            if (bracketCount == 0) break;
            pos++;
        }
        
        if (pos >= json.length()) return;
        
        std::string arrayContent = json.substr(arrayStart + 1, pos - arrayStart - 1);
        
        // Parse individual objects in the array
        size_t objStart = 0;
        while (objStart < arrayContent.length()) {
            size_t objBegin = arrayContent.find("{", objStart);
            if (objBegin == std::string::npos) break;
            
            int braceCount = 0;
            size_t objEnd = objBegin;
            
            while (objEnd < arrayContent.length()) {
                if (arrayContent[objEnd] == '{') braceCount++;
                else if (arrayContent[objEnd] == '}') braceCount--;
                
                if (braceCount == 0) break;
                objEnd++;
            }
            
            if (objEnd >= arrayContent.length()) break;
            
            std::string objContent = arrayContent.substr(objBegin, objEnd - objBegin + 1);
            parseJsonObject(objContent, sectionName);
            
            objStart = objEnd + 1;
        }
    }
    
    void parseJsonObject(const std::string& objJson, const std::string& sectionName) {
        ProgramLine line;
        line.type = sectionName;
        line.lineNumber = extractIntValue(objJson, "line");
        line.comment = extractStringValue(objJson, "comment");
        
        if (sectionName == "constants") {
            line.name = extractStringValue(objJson, "name");
            line.value = extractStringValue(objJson, "value");
        }
        else if (sectionName == "labels") {
            line.name = extractStringValue(objJson, "name");
        }
        else if (sectionName == "instructions") {
            line.mnemonic = extractStringValue(objJson, "mnemonic");
            line.operand = extractStringValue(objJson, "operand");
        }
        else if (sectionName == "data") {
            line.directive = extractStringValue(objJson, "directive");
            line.values = extractArrayValues(objJson, "values");
        }
        else if (sectionName == "directives") {
            line.name = extractStringValue(objJson, "name");
            line.operand = extractStringValue(objJson, "operand");
        }
        
        if (line.lineNumber > 0) {
            lineMap[line.lineNumber] = line;
        }
    }
    
    std::string formatForCa65(const std::string& str) {
        // Handle ca65-specific formatting requirements
        std::string formatted = str;
        
        // Remove any extra whitespace
        formatted = std::regex_replace(formatted, std::regex("\\s+"), " ");
        formatted = std::regex_replace(formatted, std::regex("^\\s+|\\s+$"), "");
        
        // Handle empty strings
        if (formatted.empty()) {
            return "";
        }
        
        // Handle quoted strings properly
        if (formatted.front() == '"' && formatted.back() == '"' && formatted.length() >= 2) {
            return formatted; // Keep quoted strings as-is
        }
        
        // Handle special ca65 syntax
        // Don't modify expressions that contain ca65 operators
        if (formatted.find_first_of("()[]{}#$%<>") != std::string::npos) {
            return formatted; // Keep ca65 expressions as-is
        }
        
        return formatted;
    }
    
    bool isInstruction(const std::string& mnemonic) {
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
        
        std::string lower_mnemonic = mnemonic;
        std::transform(lower_mnemonic.begin(), lower_mnemonic.end(), 
                      lower_mnemonic.begin(), ::tolower);
        
        return std::find(instructions.begin(), instructions.end(), lower_mnemonic) != instructions.end();
    }
    
public:
    void parseJsonFile(const std::string& filename) {
        std::ifstream file(filename);
        if (!file.is_open()) {
            throw std::runtime_error("Cannot open JSON file: " + filename);
        }
        
        std::ostringstream buffer;
        buffer << file.rdbuf();
        std::string jsonContent = buffer.str();
        
        // Parse each section
        parseJsonSection(jsonContent, "constants");
        parseJsonSection(jsonContent, "labels");
        parseJsonSection(jsonContent, "instructions");
        parseJsonSection(jsonContent, "data");
        parseJsonSection(jsonContent, "directives");
        
        // Create ordered program flow
        for (const auto& pair : lineMap) {
            programFlow.push_back(pair.second);
        }
        
        // Sort by line number
        std::sort(programFlow.begin(), programFlow.end(), 
                  [](const ProgramLine& a, const ProgramLine& b) {
                      return a.lineNumber < b.lineNumber;
                  });
    }
    
    std::string generateAssembly() {
        std::ostringstream asm_output;
        
        for (const auto& line : programFlow) {
            std::string reconstructed_line;
            
            if (line.type == "constants") {
                // ca65 constant format: NAME = VALUE
                reconstructed_line = formatForCa65(line.name + " = " + line.value);
            }
            else if (line.type == "labels") {
                // ca65 label format: LABEL: (no indentation)
                reconstructed_line = formatForCa65(line.name) + ":";
            }
            else if (line.type == "instructions") {
                // ca65 instruction format: indented mnemonic and operand
                std::string mnemonic = formatForCa65(line.mnemonic);
                std::string operand = formatForCa65(line.operand);
                
                // Skip empty instructions
                if (mnemonic.empty()) {
                    continue;
                }
                
                // Ensure proper ca65 instruction formatting
                if (isInstruction(mnemonic)) {
                    reconstructed_line = "    " + mnemonic; // 4-space indentation for instructions
                    if (!operand.empty()) {
                        reconstructed_line += " " + operand;
                    }
                } else {
                    // If not recognized as instruction, treat as is
                    reconstructed_line = "    " + mnemonic;
                    if (!operand.empty()) {
                        reconstructed_line += " " + operand;
                    }
                }
            }
            else if (line.type == "data") {
                // ca65 data directive format
                std::string directive = formatForCa65(line.directive);
                
                // Skip empty data directives
                if (directive.empty()) {
                    continue;
                }
                
                reconstructed_line = "    " + directive; // Indent data directives
                
                if (!line.values.empty()) {
                    // Filter out empty values and build the values string
                    std::vector<std::string> validValues;
                    for (const auto& value : line.values) {
                        std::string formattedValue = formatForCa65(value);
                        if (!formattedValue.empty()) {
                            validValues.push_back(formattedValue);
                        }
                    }
                    
                    if (!validValues.empty()) {
                        reconstructed_line += " ";
                        for (size_t i = 0; i < validValues.size(); ++i) {
                            if (i > 0) reconstructed_line += ", ";
                            reconstructed_line += validValues[i];
                        }
                    }
                }
            }
            else if (line.type == "directives") {
                // ca65 assembler directive format (usually not indented)
                std::string directive = formatForCa65(line.name);
                std::string operand = formatForCa65(line.operand);
                
                // Skip empty directives
                if (directive.empty()) {
                    continue;
                }
                
                // Most ca65 directives start with . and are not indented
                if (directive.front() == '.') {
                    reconstructed_line = directive;
                } else {
                    reconstructed_line = "    " + directive; // Indent if not a standard directive
                }
                
                if (!operand.empty()) {
                    reconstructed_line += " " + operand;
                }
            }
            
            // Add comment if present (ca65 uses ; for comments)
            if (!line.comment.empty()) {
                std::string comment = formatForCa65(line.comment);
                if (!reconstructed_line.empty()) {
                    // Align comments at a consistent column (e.g., column 40)
                    while (reconstructed_line.length() < 40) {
                        reconstructed_line += " ";
                    }
                    reconstructed_line += "; " + comment;
                } else {
                    // Comment-only line
                    reconstructed_line = "; " + comment;
                }
            }
            
            // Only output non-empty lines
            if (!reconstructed_line.empty()) {
                asm_output << reconstructed_line << "\n";
            }
        }
        
        return asm_output.str();
    }
};

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <input.json> <output.asm>" << std::endl;
        std::cerr << "Converts JSON assembly format back to ca65-compatible assembly source" << std::endl;
        return 1;
    }
    
    try {
        JsonToAssemblyConverter converter;
        converter.parseJsonFile(argv[1]);
        
        std::string asmOutput = converter.generateAssembly();
        
        std::ofstream outputFile(argv[2]);
        if (!outputFile.is_open()) {
            throw std::runtime_error("Cannot create output file: " + std::string(argv[2]));
        }
        
        outputFile << asmOutput;
        outputFile.close();
        
        std::cout << "Successfully converted " << argv[1] << " to ca65-compatible " << argv[2] << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
