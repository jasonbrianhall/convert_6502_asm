#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <regex>

// Directory creation
#ifdef _WIN32
    #include <direct.h>
#else
    #include <sys/stat.h>
    #include <sys/types.h>
#endif

struct JsonInstruction {
    std::string mnemonic;
    std::string operand;
    std::string comment;
    int lineNumber;
};

struct JsonData {
    std::string directive;
    std::string type;
    std::vector<std::string> values;
    std::string comment;
    int lineNumber;
};

struct JsonLabel {
    std::string name;
    std::string comment;
    int lineNumber;
};

struct JsonConstant {
    std::string name;
    std::string value;
    std::string comment;
    int lineNumber;
};

struct JsonDirective {
    std::string name;
    std::string operand;
    std::string comment;
    int lineNumber;
};

struct ProgramFlowItem {
    std::string type;
    std::string content;
    std::string comment;
    int lineNumber;
};

class JsonToCppConverter {
private:
    std::vector<JsonConstant> constants;
    std::vector<JsonLabel> labels;
    std::vector<JsonInstruction> instructions;
    std::vector<JsonData> data;
    std::vector<JsonDirective> directives;
    std::vector<ProgramFlowItem> programFlow;
    std::map<int, std::string> commentMap;
    
    int returnLabelIndex = 0;
    
    std::string unescapeJson(const std::string& str) {
        std::string unescaped;
        for (size_t i = 0; i < str.length(); ++i) {
            if (str[i] == '\\' && i + 1 < str.length()) {
                switch (str[i + 1]) {
                    case '"': unescaped += '"'; i++; break;
                    case '\\': unescaped += '\\'; i++; break;
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
            if (json[endQuote] == '"' && (endQuote == 0 || json[endQuote - 1] != '\\')) {
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
        
        size_t pos = 0;
        while (pos < arrayContent.length()) {
            while (pos < arrayContent.length() && (arrayContent[pos] == ' ' || 
                   arrayContent[pos] == '\t' || arrayContent[pos] == ',' || 
                   arrayContent[pos] == '\n' || arrayContent[pos] == '\r')) {
                pos++;
            }
            
            if (pos >= arrayContent.length()) break;
            
            if (arrayContent[pos] == '"') {
                size_t startQuote = pos;
                size_t endQuote = startQuote + 1;
                while (endQuote < arrayContent.length()) {
                    if (arrayContent[endQuote] == '"' && (endQuote == 0 || arrayContent[endQuote - 1] != '\\')) {
                        break;
                    }
                    endQuote++;
                }
                
                if (endQuote < arrayContent.length()) {
                    values.push_back(unescapeJson(arrayContent.substr(startQuote + 1, endQuote - startQuote - 1)));
                    pos = endQuote + 1;
                } else {
                    break;
                }
            } else {
                size_t valueStart = pos;
                while (pos < arrayContent.length() && arrayContent[pos] != ',' && 
                       arrayContent[pos] != ']' && arrayContent[pos] != '\n') {
                    pos++;
                }
                std::string value = arrayContent.substr(valueStart, pos - valueStart);
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
        
        while (pos < json.length()) {
            if (json[pos] == '[') bracketCount++;
            else if (json[pos] == ']') bracketCount--;
            if (bracketCount == 0) break;
            pos++;
        }
        
        if (pos >= json.length()) return;
        
        std::string arrayContent = json.substr(arrayStart + 1, pos - arrayStart - 1);
        
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
        if (sectionName == "constants") {
            JsonConstant constant;
            constant.name = extractStringValue(objJson, "name");
            constant.value = extractStringValue(objJson, "value");
            constant.comment = extractStringValue(objJson, "comment");
            constant.lineNumber = extractIntValue(objJson, "line");
            constants.push_back(constant);
        }
        else if (sectionName == "labels") {
            JsonLabel label;
            label.name = extractStringValue(objJson, "name");
            label.comment = extractStringValue(objJson, "comment");
            label.lineNumber = extractIntValue(objJson, "line");
            labels.push_back(label);
        }
        else if (sectionName == "instructions") {
            JsonInstruction instruction;
            instruction.mnemonic = extractStringValue(objJson, "mnemonic");
            instruction.operand = extractStringValue(objJson, "operand");
            instruction.comment = extractStringValue(objJson, "comment");
            instruction.lineNumber = extractIntValue(objJson, "line");
            instructions.push_back(instruction);
        }
        else if (sectionName == "data") {
            JsonData dataItem;
            dataItem.directive = extractStringValue(objJson, "directive");
            dataItem.type = extractStringValue(objJson, "type");
            dataItem.values = extractArrayValues(objJson, "values");
            dataItem.comment = extractStringValue(objJson, "comment");
            dataItem.lineNumber = extractIntValue(objJson, "line");
            data.push_back(dataItem);
        }
        else if (sectionName == "directives") {
            JsonDirective directive;
            directive.name = extractStringValue(objJson, "name");
            directive.operand = extractStringValue(objJson, "operand");
            directive.comment = extractStringValue(objJson, "comment");
            directive.lineNumber = extractIntValue(objJson, "line");
            directives.push_back(directive);
        }
        else if (sectionName == "program_flow") {
            ProgramFlowItem item;
            item.type = extractStringValue(objJson, "type");
            item.content = extractStringValue(objJson, "content");
            item.comment = extractStringValue(objJson, "comment");
            item.lineNumber = extractIntValue(objJson, "line");
            programFlow.push_back(item);
        }
    }
    
    // Based on translator.cpp translateExpression patterns
    std::string translateExpression(const std::string& expr) {
        if (expr.empty()) return "";
        
        // Handle hex constants: $FF -> 0xFF
        if (expr[0] == '$') {
            return "0x" + expr.substr(1);
        }
        
        // Handle binary constants: %11001100 -> BOOST_BINARY(11001100)
        if (expr[0] == '%') {
            return "BOOST_BINARY(" + expr.substr(1) + ")";
        }
        
        // Handle decimal constants and names as-is
        return expr;
    }
    
    // Based on translator.cpp translateOperand patterns
    std::string translateOperand(const std::string& operand) {
        if (operand.empty()) return "";
        
        // Handle immediate addressing: #value -> value
        if (operand[0] == '#') {
            return translateExpression(operand.substr(1));
        }
        
        // Handle indirect addressing: (value) -> M(value)
        if (operand.front() == '(' && operand.back() == ')') {
            std::string inner = operand.substr(1, operand.length() - 2);
            return "M(" + translateExpression(inner) + ")";
        }
        
        // Handle indexed addressing: value,x -> value + x
        size_t commaPos = operand.find(',');
        if (commaPos != std::string::npos) {
            std::string base = operand.substr(0, commaPos);
            std::string index = operand.substr(commaPos + 1);
            
            // Trim whitespace
            base = std::regex_replace(base, std::regex("^\\s+|\\s+$"), "");
            index = std::regex_replace(index, std::regex("^\\s+|\\s+$"), "");
            
            // Special case for (value),y -> W(value) + y
            if (base.front() == '(' && base.back() == ')' && index == "y") {
                std::string inner = base.substr(1, base.length() - 2);
                return "W(" + translateExpression(inner) + ") + y";
            }
            
            return translateExpression(base) + " + " + index;
        }
        
        // Everything else needs memory access: value -> M(value)
        return "M(" + translateExpression(operand) + ")";
    }
    
    // Based on translator.cpp translateBranch pattern
    std::string translateBranch(const std::string& condition, const std::string& destination) {
        return "if (" + condition + ")\n        goto " + destination + ";";
    }
    
    // Based on translator.cpp translateInstruction patterns
    std::string translateInstruction(const JsonInstruction& inst) {
        std::string mnemonic = inst.mnemonic;
        std::string operand = inst.operand;
        
        // Load instructions
        if (mnemonic == "lda") return "a = " + translateOperand(operand) + ";";
        if (mnemonic == "ldx") return "x = " + translateOperand(operand) + ";";
        if (mnemonic == "ldy") return "y = " + translateOperand(operand) + ";";
        
        // Store instructions
        if (mnemonic == "sta") return "writeData(" + translateExpression(operand) + ", a);";
        if (mnemonic == "stx") return "writeData(" + translateExpression(operand) + ", x);";
        if (mnemonic == "sty") return "writeData(" + translateExpression(operand) + ", y);";
        
        // Transfer instructions
        if (mnemonic == "tax") return "x = a;";
        if (mnemonic == "tay") return "y = a;";
        if (mnemonic == "txa") return "a = x;";
        if (mnemonic == "tya") return "a = y;";
        if (mnemonic == "tsx") return "x = s;";
        if (mnemonic == "txs") return "s = x;";
        
        // Stack instructions
        if (mnemonic == "pha") return "pha();";
        if (mnemonic == "php") return "php();";
        if (mnemonic == "pla") return "pla();";
        if (mnemonic == "plp") return "plp();";
        
        // Logical instructions
        if (mnemonic == "and") return "a &= " + translateOperand(operand) + ";";
        if (mnemonic == "eor") return "a ^= " + translateOperand(operand) + ";";
        if (mnemonic == "ora") return "a |= " + translateOperand(operand) + ";";
        if (mnemonic == "bit") return "bit(" + translateOperand(operand) + ");";
        
        // Arithmetic instructions
        if (mnemonic == "adc") return "a += " + translateOperand(operand) + ";";
        if (mnemonic == "sbc") return "a -= " + translateOperand(operand) + ";";
        
        // Compare instructions
        if (mnemonic == "cmp") return "compare(a, " + translateOperand(operand) + ");";
        if (mnemonic == "cpx") return "compare(x, " + translateOperand(operand) + ");";
        if (mnemonic == "cpy") return "compare(y, " + translateOperand(operand) + ");";
        
        // Increment/Decrement
        if (mnemonic == "inc") return "++" + translateOperand(operand) + ";";
        if (mnemonic == "inx") return "++x;";
        if (mnemonic == "iny") return "++y;";
        if (mnemonic == "dec") return "--" + translateOperand(operand) + ";";
        if (mnemonic == "dex") return "--x;";
        if (mnemonic == "dey") return "--y;";
        
        // Shift instructions
        if (mnemonic == "asl") {
            if (operand.empty()) return "a <<= 1;";
            return translateOperand(operand) + " <<= 1;";
        }
        if (mnemonic == "lsr") {
            if (operand.empty()) return "a >>= 1;";
            return translateOperand(operand) + " >>= 1;";
        }
        if (mnemonic == "rol") {
            if (operand.empty()) return "a.rol();";
            return translateOperand(operand) + ".rol();";
        }
        if (mnemonic == "ror") {
            if (operand.empty()) return "a.ror();";
            return translateOperand(operand) + ".ror();";
        }
        
        // Jump instructions
        if (mnemonic == "jmp") {
            if (operand == "EndlessLoop") return "return;";
            return "goto " + operand + ";";
        }
        
        if (mnemonic == "jsr") {
            if (operand == "JumpEngine") {
                // Special case - would need more context to implement properly
                return "/* JSR JumpEngine - needs jump table implementation */";
            }
            return "JSR(" + operand + ", " + std::to_string(returnLabelIndex++) + ");";
        }
        
        if (mnemonic == "rts") return "goto Return;";
        
        // Branch instructions
        if (mnemonic == "bcc") return translateBranch("!c", operand);
        if (mnemonic == "bcs") return translateBranch("c", operand);
        if (mnemonic == "beq") return translateBranch("z", operand);
        if (mnemonic == "bmi") return translateBranch("n", operand);
        if (mnemonic == "bne") return translateBranch("!z", operand);
        if (mnemonic == "bpl") return translateBranch("!n", operand);
        if (mnemonic == "bvc") return translateBranch("!v", operand);
        if (mnemonic == "bvs") return translateBranch("v", operand);
        
        // Flag instructions
        if (mnemonic == "clc") return "c = 0;";
        if (mnemonic == "cld") return "/* cld */";
        if (mnemonic == "cli") return "/* cli */";
        if (mnemonic == "clv") return "/* clv */";
        if (mnemonic == "sec") return "c = 1;";
        if (mnemonic == "sed") return "/* sed */";
        if (mnemonic == "sei") return "/* sei */";
        
        // Misc instructions
        if (mnemonic == "brk") return "/* brk */";
        if (mnemonic == "nop") return "; // nop";
        if (mnemonic == "rti") return "return;";
        
        return "/* Unknown instruction: " + mnemonic + " */";
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
        
        // Parse all sections
        parseJsonSection(jsonContent, "constants");
        parseJsonSection(jsonContent, "labels");
        parseJsonSection(jsonContent, "instructions");
        parseJsonSection(jsonContent, "data");
        parseJsonSection(jsonContent, "directives");
        parseJsonSection(jsonContent, "program_flow");
        
        // Build comment map for line number lookups
        for (const auto& item : programFlow) {
            if (!item.comment.empty()) {
                commentMap[item.lineNumber] = item.comment;
            }
        }
    }
    
    void generateCppFiles(const std::string& outputDir) {
        // Create output directory
        #ifdef _WIN32
            _mkdir(outputDir.c_str());
        #else
            mkdir(outputDir.c_str(), 0755);
        #endif
        
        generateConstantHeader(outputDir);
        generateSourceFile(outputDir);
        generateDataFiles(outputDir);
        
        std::cout << "Generated C++ files in " << outputDir << ":" << std::endl;
        std::cout << "  SMB.cpp" << std::endl;
        std::cout << "  SMBData.cpp" << std::endl;
        std::cout << "  SMBDataPointers.hpp" << std::endl;
        std::cout << "  SMBConstants.hpp" << std::endl;
    }
    
private:
    void generateConstantHeader(const std::string& outputDir) {
        std::ofstream file(outputDir + "/SMBConstants.hpp");
        
        file << "// This is an automatically generated file.\n";
        file << "// Do not edit directly.\n//\n";
        file << "#ifndef SMBCONSTANTS_HPP\n";
        file << "#define SMBCONSTANTS_HPP\n\n";
        
        for (const auto& constant : constants) {
            file << "#define " << constant.name << " " << translateExpression(constant.value);
            if (!constant.comment.empty()) {
                file << " // " << constant.comment;
            }
            file << "\n";
        }
        
        file << "\n#endif // SMBCONSTANTS_HPP\n";
    }
    
    void generateSourceFile(const std::string& outputDir) {
        std::ofstream file(outputDir + "/SMB.cpp");
        
        file << "// This is an automatically generated file.\n";
        file << "// Do not edit directly.\n//\n";
        file << "#include \"SMB.hpp\"\n\n";
        
        file << "void SMBEngine::code(int mode)\n{\n";
        file << "    switch (mode)\n    {\n";
        file << "    case 0:\n";
        file << "        loadConstantData();\n";
        file << "        goto Start;\n";
        file << "    case 1:\n";
        file << "        goto NonMaskableInterrupt;\n";
        file << "    }\n\n";
        
        // Group program flow items by labels
        std::string currentLabel;
        std::vector<ProgramFlowItem> currentItems;
        
        for (const auto& item : programFlow) {
            if (item.type == "label") {
                // Process previous label if any
                if (!currentLabel.empty()) {
                    generateLabelCode(file, currentLabel, currentItems);
                }
                
                currentLabel = item.content;
                currentItems.clear();
            } else {
                currentItems.push_back(item);
            }
        }
        
        // Process final label
        if (!currentLabel.empty()) {
            generateLabelCode(file, currentLabel, currentItems);
        }
        
        // Generate return handler
        file << "// Return handler\n";
        file << "// This emulates the RTS instruction using a generated jump table\n//\n";
        file << "Return:\n";
        file << "    switch (popReturnIndex())\n    {\n";
        
        for (int i = 0; i < returnLabelIndex; i++) {
            file << "    case " << i << ":\n";
            file << "        goto Return_" << i << ";\n";
        }
        
        file << "    }\n";
        file << "}\n";
    }
    
    void generateLabelCode(std::ofstream& file, const std::string& labelName, 
                          const std::vector<ProgramFlowItem>& items) {
        // Remove trailing colon from label name if present, then add it back for C++
        std::string cleanLabelName = labelName;
        if (cleanLabelName.back() == ':') {
            cleanLabelName = cleanLabelName.substr(0, cleanLabelName.length() - 1);
        }
        
        file << "\n" << cleanLabelName << ":";
        
        // Add comment if label has one
        auto labelIt = std::find_if(labels.begin(), labels.end(),
            [&cleanLabelName](const JsonLabel& l) { return l.name == cleanLabelName; });
        if (labelIt != labels.end() && !labelIt->comment.empty()) {
            file << " // " << labelIt->comment;
        }
        file << "\n";
        
        for (const auto& item : items) {
            if (item.type == "instruction") {
                // Find the instruction details
                auto instIt = std::find_if(instructions.begin(), instructions.end(),
                    [&item](const JsonInstruction& inst) { 
                        return inst.lineNumber == item.lineNumber; 
                    });
                
                if (instIt != instructions.end()) {
                    file << "    " << translateInstruction(*instIt);
                    if (!item.comment.empty()) {
                        file << " // " << item.comment;
                    }
                    file << "\n";
                    
                    // Add separator after RTS
                    if (instIt->mnemonic == "rts") {
                        file << "\n//------------------------------------------------------------------------\n";
                    }
                }
            } else if (item.type == "data") {
                file << "    /* Data: " << item.content << " */";
                if (!item.comment.empty()) {
                    file << " // " << item.comment;
                }
                file << "\n";
            }
        }
    }
    
    void generateDataFiles(const std::string& outputDir) {
        // Generate data pointers header
        std::ofstream headerFile(outputDir + "/SMBDataPointers.hpp");
        headerFile << "// This is an automatically generated file.\n";
        headerFile << "// Do not edit directly.\n//\n";
        headerFile << "#ifndef SMBDATAPOINTERS_HPP\n";
        headerFile << "#define SMBDATAPOINTERS_HPP\n\n";
        
        headerFile << "struct SMBDataPointers\n{\n";
        
        // Generate data loading code
        std::ofstream dataFile(outputDir + "/SMBData.cpp");
        dataFile << "// This is an automatically generated file.\n";
        dataFile << "// Do not edit directly.\n//\n";
        dataFile << "#include \"SMB.hpp\"\n\n";
        dataFile << "void SMBEngine::loadConstantData()\n{\n";
        
        std::ostringstream addressDefaults;
        addressDefaults << "    SMBDataPointers()\n    {\n";
        
        int storageAddress = 0x8000;
        
        // Process data sections
        for (const auto& dataItem : data) {
            if (dataItem.directive == ".db" || dataItem.directive == ".byte") {
                // Find corresponding label
                std::string labelName = "UnknownData";
                for (const auto& item : programFlow) {
                    if (item.lineNumber == dataItem.lineNumber && 
                        item.lineNumber > 0) {
                        // Look backwards for the label
                        for (auto it = programFlow.rbegin(); it != programFlow.rend(); ++it) {
                            if (it->lineNumber < dataItem.lineNumber && it->type == "label") {
                                labelName = it->content;
                                break;
                            }
                        }
                        break;
                    }
                }
                
                // Remove trailing colon
                if (labelName.back() == ':') {
                    labelName = labelName.substr(0, labelName.length() - 1);
                }
                
                // Generate data array
                dataFile << "    // " << labelName << "\n";
                dataFile << "    const uint8_t " << labelName << "_data[] = {\n        ";
                
                for (size_t i = 0; i < dataItem.values.size(); ++i) {
                    if (i > 0) dataFile << ", ";
                    dataFile << translateExpression(dataItem.values[i]);
                }
                
                dataFile << "\n    };\n";
                dataFile << "    writeData(" << labelName << ", " << labelName 
                         << "_data, sizeof(" << labelName << "_data));\n\n";
                
                // Generate pointers
                headerFile << "    uint16_t " << labelName << "_ptr;\n";
                addressDefaults << "        this->" << labelName << "_ptr = 0x" 
                               << std::hex << storageAddress << std::dec << ";\n";
                
                storageAddress += dataItem.values.size();
            }
        }
        
        headerFile << "    uint16_t freeSpaceAddress;\n";
        addressDefaults << "        this->freeSpaceAddress = 0x" << std::hex 
                       << storageAddress << std::dec << ";\n";
        addressDefaults << "    }\n";
        
        headerFile << "\n" << addressDefaults.str() << "};\n\n";
        headerFile << "#endif // SMBDATAPOINTERS_HPP\n";
        
        dataFile << "}\n";
    }
};

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <input.json> <output_directory>" << std::endl;
        std::cerr << "Converts JSON assembly format to C++ code" << std::endl;
        return 1;
    }
    
    try {
        JsonToCppConverter converter;
        converter.parseJsonFile(argv[1]);
        converter.generateCppFiles(argv[2]);
        
        std::cout << "Successfully converted " << argv[1] << " to C++ in " << argv[2] << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
