/*  AEC - Assembly Error Checker - ARM Assembly Errors
    Authors: Benjamin Robinson, Adam Barr, Jennifer McCain, Michael Fechter - Team 10
    Last Edited: 3/19/2024
    Edited in VSCode on Windows in C++

    AEC is a command line executable that provides error checking and stasitical
    analysis to assembly files. AEC operates entirely off the command line using
    unix command codes. Files are processed by taking each line of the program and 
    turning it into tokens that can be either saved or used to activate flags. 
    These saved tokens and flags are then used to determine if an error has occured 
    or to determine metrics like Halstead's or Cyclomatic Complexity.
*/

#include <iostream>
#include <fstream>
#include <filesystem>
#include <unordered_set>
#include <cmath>
#include <vector>
#include <string>
#include <algorithm>
#include <unordered_map>
#include <sys/stat.h>
#include <ctime>

void fileReader(std::string, std::string, int);

/*************************************************************************
 * main takes the command line given by the user and calls filereader
 * based on the commands given in the command line. */
int main(int argc, char* argv[]) {
    // Before ANYTHING is done, we check that we have correct input
    // Format for command is AEC <filename> <command> or AEC <directory> -t
    if (argc != 3) 
    {
        std::cerr << "Correct formats: AEC <filename> <command> || AEC <directory> -t\n";
        return -1;
    }

    std::string input_file = argv[1];
    std::string output_file = "Reports/" + input_file.substr(0, input_file.find_last_of(".")) + "_report.txt";
    std::string command = argv[2];


    // Commands: -h help, -m print metrics to console, -e print errors to console
    // -r print report, -t reads folder for all .s files and makes reports
    // -c makes csv individual file -v reads a folder of .s files and makes csv files
    switch(command[1]) 
    {
        case 'h':
            std::cout << "commands:\n";
            std::cout << "  -h\t\tDisplay help message\n";
            std::cout << "  -m\t\tPrint metrics to terminal\n";
            std::cout << "  -e\t\tPrint errors to terminal\n";
            std::cout << "  -r\t\tCreate report file\n";
            std::cout << "  -c\t\tCreate csv file\n";
            std::cout << "  <folder path> -t\t\tCreate report files from folder\n";
            std::cout << "  <folder path> -v\t\tCreate csv files from folder\n";

            return 0;

        case 'm': // Print to console
            fileReader(input_file, output_file, 1);
            return 0;

        case 'e': // Print to console
            fileReader(input_file, output_file, 2);
            return 0;

        case 'r': // Print to file
            std::filesystem::create_directory("Reports");
            fileReader(input_file, output_file, 3);

            return 0;

        case 't':
            // Makes a folder within the directory
            std::filesystem::create_directory("Reports");

            // Iteratively looks through a folder for all the .s files and
            // sends each one individually to fileReader
            for (auto& file : std::filesystem::directory_iterator(input_file)) 
            {
                if (file.is_regular_file() && file.path().extension() == ".s") 
                {
                    input_file = file.path().string();
                    output_file = "Reports/" + file.path().stem().string() + "_report.txt";

                    fileReader(input_file, output_file, 3);
                }
            }

            return 0;

        case 'c':
            output_file = "AEC_Dataset.csv";
            fileReader(input_file, output_file, 4);

            return 0;

        case 'v':
            // Iteratively looks through a folder for all the .s files and
            // sends each one individually to fileReader
            output_file = "AEC_Dataset.csv";

            for (auto& file : std::filesystem::directory_iterator(input_file)) 
            {
                if (file.is_regular_file() && file.path().extension() == ".s") 
                {
                    input_file = file.path().string();
                    fileReader(input_file, output_file, 4);
                }
            }

            return 0;

        default:
            std::cerr << "Error: AEC <filename> -h for help\n";

            return -1;
    }

    return 0;
}

/***************************************************************************
 * filereader takes a file and command given by main to begin analysis of
 * that .s file. filereader operates by turning lines into tokens which
 * can be used to determine if errors have occured or to determine
 * statistical data about the file. filereader then reports the data
 * determined by the variable "command" */
void fileReader(std::string input_file, std::string output_file, int command) {
    std::unordered_set<std::string> unwantedOperators = {
        "SWI", "LDM", "LTM", "swi", "ldm", "ltm"};
    std::unordered_set<std::string> registers = {
        "r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7", "r8", "r9", "r10", "r11", "r12",
        "R0", "R1", "R2", "R3", "R4", "R5", "R6", "R7", "R8", "R9", "R10", "R11", "R12",
        "r13", "r14", "r15", "R13", "R14", "R15"};
    std::unordered_set<std::string> restrictedRegisters = {
        "r13", "r14", "r15", "R13", "R14", "R15"};
    std::unordered_set<std::string> compareList = {
        "EQ", "eq", "NE", "ne", "GE", "ge", "LT", "lt", "GT", "gt", "LE", "le", "CS", "cs", "CC", 
        "cc", "MI", "mi", "PL", "pl", "VS", "vs", "VC", "vc", "HI", "hi", "LS", "ls", "AL", "al"};
    std::unordered_set<std::string> uniqueOperands, uniqueOperators, subroutines;
    std::unordered_set<int> register0Use, register1Use, register2Use, register3Use;
    std::unordered_set<int> register4Use, register5Use, register6Use, register7Use;
    std::unordered_set<int> register8Use, register9Use, register10Use, register11Use, register12Use;
    std::unordered_set<int> register13Use, register14Use, register15Use;
    std::vector<std::string> labels, variables, constants;
    std::vector<std::string> stringError, unwantedInstructions, branchUse;
    std::vector<std::string> svcUse, subroutineUse, isolatedCode;
    std::vector<std::string> unusedConditional, unusedLabel, unusedVariable, unusedConstant;
    std::vector<std::string> restrictedError, noReturnError, lrSaveError;
    std::vector<std::string> branchOutError, registerError;
    std::vector<std::string> indirectMode, indirectOffsetMode, preIndexMode;
    std::vector<std::string> postIndexMode, pcRelativeMode, pcLiteralMode, unsureMode;
    std::string line, token, subtoken, linePreComment;
    std::ifstream infile(input_file);
    std::vector<int> labelLineNum, returnLineNum, blCallLineNum; 
    std::vector<int> lrSaveLineNum, badBranchLineNum, sorter;
    std::unordered_map<std::string, std::vector<int>> directiveUse;
    int fullCommentLines = 0, blankLines = 0, totalLines = 0;
    int linesWComment = 0, linesWOComment = 0, dirLines = 0;
    int totalOperators = 0, totalOperands = 0;
    int cyclomatic = 1, commentPos, cmpLine, dataLineNum, nextLabel;
    int pushNum = 0, popNum = 0, numTokens = 0;
    bool operatorFlag = false, branchFlag = false, movFlag = false;
    bool globalFlag = false, dataFlag = false, globalErrorFlag = false;
    bool checkSVC = false, exitExists = false, dataExists = false;
    bool restrictedRegisterFlag = false, pushFlag = false, equFlag = false;
    bool blBranchFlag = false, noReturnBranch = false, cmpNextLine = false;
    bool subroutineFlag = false, returnFlag = false, bxBranchFlag = false;
    bool subroutineCall = false, lrSaved = false, popFlag = false;
    bool ldrFlag = false, strFlag = false, movPCFlag = false;
    bool r0Flag = false, r1Flag = false, r2Flag = false, r3Flag = false, r4Flag = false;
    bool r5Flag = false, r6Flag = false, r7Flag = false, r8Flag = false;
    bool r9Flag = false, r10Flag = false, r11Flag = false, r12Flag = false;
    bool r0ErrorFlag = false, r1ErrorFlag = false, r2ErrorFlag = false;
    bool r3ErrorFlag = false, r4ErrorFlag = false, r5ErrorFlag = false; 
    bool r6ErrorFlag = false, r7ErrorFlag = false, r8ErrorFlag = false;
    bool r9ErrorFlag = false, r10ErrorFlag = false, r11ErrorFlag = false, r12ErrorFlag = false;

    if (!infile.is_open())  // Check if file successfully opened
    {
        std::cerr << "Error: Failed to open file: " << input_file;
        exit(-1);
    }

    /***************************************************************************
     * This sections reads and stores the file in a vector of strings
     * It also collects all the labels and custom variables for later analysis*/
    while (std::getline(infile, line)) 
    {
        // Push the new line into a vector of strings to be able to test it later
        // Also establish each new lines flags
        totalLines++;
        operatorFlag = false;
        restrictedRegisterFlag = false;
        branchFlag = false;
        blBranchFlag = false;
        bxBranchFlag = false;
        pushFlag = false;
        movFlag = false;
        ldrFlag = false;
        strFlag = false;
        equFlag = false;
        movPCFlag = false;
        popFlag = false;
        numTokens = 0;
        r0ErrorFlag = false;
        r1ErrorFlag = false;
        r2ErrorFlag = false;
        r3ErrorFlag = false;
        r4ErrorFlag = false;
        r5ErrorFlag = false;
        r6ErrorFlag = false;
        r7ErrorFlag = false;
        r8ErrorFlag = false;
        r9ErrorFlag = false;
        r10ErrorFlag = false;
        r11ErrorFlag = false;
        r12ErrorFlag = false;
        std::istringstream iss(line);    // Grab token of line to check first token
        iss >> token;  

        /**************************************************************************
         * A line can be empty, a full comment, or have functional code in it
         * From there it can have a comment or not */
        if (line.find_first_not_of(' ') == std::string::npos) 
        {
            blankLines++;
        } 
        else if (token[0] == '@' || token[0] == '/')
        {
            fullCommentLines++;
        }
        else
        {
            if (line.find("@") != std::string::npos || line.find("/") != std::string::npos) 
            {
                linesWComment++;
            } 
            else 
            {
                linesWOComment++;
            }

            /*********************************************************************************
             * Comments don't need to be tokenized so we make sublines without them.
             * The example programs have comments with @ and / * so we check for both. */
            if(line.find('@') != std::string::npos) commentPos = line.find('@'); // End at @
            else if(line.find('/') != std::string::npos) commentPos = line.find('/'); // End at /
            else commentPos = line.size();  // If no comment is detected grab full line
            linePreComment = line.substr(0, commentPos);    // Get rid of comment

            std::istringstream issToken(linePreComment);    // Grab tokens of the uncommented line

            while (issToken >> token)   // While there are still tokens on the line
            {
                if (!token.empty()) 
                {
                    numTokens++; // Placement checker for tokens

                    /***************************************************************************************
                     * A token can be identified by its position and by its contents. A token in the first
                     * position that lacks any other defining elements, like : or . is an operator. This
                     * kind of logic prevents the need for sets which may not hold all available operators.*/
                    if (numTokens == 1 && token[0] != '.' && token.back() != ':')
                    {
                        totalOperators++;               // Halstead's total operators
                        uniqueOperators.insert(token);  // Halstead's unique operators
                        operatorFlag = true;    // Establish that the rest of the tokens in this line are operands

                        /*****************************************************************************************
                         * cmpNextLine means the previous lines operator was cmp. We then check individually that
                         * the token, the next operator contains a conditional flag to validate the need for the
                         *  cmp operator. */
                        if(cmpNextLine == true)
                        {
                            if(token.length() > 2)  // Don't want to grab subtoken of 2 if token isn't large enough
                            {
                                subtoken = token.substr(token.length() - 2, 2);   // Grab only the last 2 characters of the token
                                if(compareList.find(subtoken) == compareList.end()) // Check against list of comparison commands
                                {
                                    unusedConditional.push_back("Condition flag updated but unused at line " + std::to_string(cmpLine - 1));
                                }

                            }
                            else    // If token isn't large enough, it doesn't have a conditional element
                            {
                                unusedConditional.push_back("Condition flag updated but unused at line " + std::to_string(cmpLine));
                            }

                            cmpNextLine = false;
                        }
                        /*****************************************************************
                         * noReturnBranch stores an error if there was an operator used
                         * after a b branch but before a new label*/
                        if(noReturnBranch == true)
                        {
                            isolatedCode.push_back("Code after unconditional branch at line " + std::to_string(totalLines));
                        }
                        /*****************************************************************
                         * if the first character of an operator is a b then the operator
                         * is a branch. This is a stylistic choice of arm assembly.
                         * Once we determine an operator is a branch we set a flag to
                         * identify the following operands as being part of a branch.*/
                        else if (token[0] == 'b' || token[0] == 'B') // Check if operator is a branch
                        {
                            cyclomatic++;   // Cyclomatic complexitiy is 1 + # of branches
                            branchFlag = true;
                            if(token == "bl" || token == "BL")
                            {
                                blBranchFlag = true;
                            }
                            else if(token == "bx" || token == "BX")
                            {
                                bxBranchFlag = true;
                            }
                            else if(token == "b" || token == "B")
                            {   // Once a b branch is done, any code until next label is isolated
                                noReturnBranch = true;
                            }
                        }
                        /*****************************************************************
                         * Unwantedoperators are a list of operators we don't expect the
                         * student to use. */
                        else if(unwantedOperators.find(token) != unwantedOperators.end())
                        {
                            unwantedInstructions.push_back("Unexpected instruction at line " + std::to_string(totalLines));
                        }
                        /************************************************************************
                         * If a value is being loaded then the following operands could
                         * include the registers that are not for standard use: r13, r14, r15 */
                        else if(token.find("ldr") != std::string::npos || token.find("LDR") != std::string::npos) 
                        {
                            restrictedRegisterFlag = true; // Check all operands on this line
                            ldrFlag = true;
                        }
                        /****************************************************************
                         * Same as LDR                        */
                        else if(token.find("mov") != std::string::npos || token.find("MOV") != std::string::npos)
                        {
                            restrictedRegisterFlag = true; // Check all operands on this line
                            movFlag = true;
                        }
                        /************************************************************
                         * Checks the next operands for addressing modes  */
                        else if(token.find("str") != std::string::npos || token.find("STR") != std::string::npos)
                        {
                            strFlag = true;
                        }
                        /*****************************************************************
                         * If operator is svc then we check that the following operand 
                         * is 0 to validate an exit from the program. */
                        else if((token.find("svc") != std::string::npos || token.find("SVC") != std::string::npos) && dataFlag != true)
                        {
                            checkSVC = true;
                        }
                        /*****************************************************************
                         * Check the next line for use of the comparison flag update */
                        else if(token == "cmp" || token == "CMP")
                        {
                            cmpNextLine = true;
                            cmpLine = totalLines;
                        }
                        /****************************************************************
                         * Check if the LR is saved via Push                        */
                        else if(token.find("push") != std::string::npos || token.find("PUSH") != std::string::npos)
                        {
                            pushFlag = true;
                        }
                        else if(token.find("pop") != std::string::npos || token.find("POP") != std::string::npos)
                        {
                            popFlag = true;
                        }
                    }
                    /*****************************************************************
                     * A token following an operator is an operand. It can be a
                     * register, a defined value, or a literal. */
                    else if (operatorFlag == true) 
                    {
                        totalOperands++;
                        /******************************************************************************
                         * Validating uniqueness of operands by removing , [ ] and []
                         * Then each operand is stored in an unordered set to ignore multiple entries*/
                        if(token.find(",") != std::string::npos && token.find("[") == std::string::npos
                            && token.find("{") == std::string::npos)
                        {   // First is if operand has only , like r1,
                            subtoken = token;   // Don't ever edit token directly
                            subtoken.erase(subtoken.size() - 1); // Remove comma
                            uniqueOperands.insert(subtoken);
                        }
                        else if(token.find("[") != std::string::npos && token.find("]") == std::string::npos
                        && token.find(",") != std::string::npos)
                        {   // Second is [r1,
                            subtoken = token;   // Don't ever edit token directly
                            subtoken.erase(subtoken.size() - 1); // Remove ,
                            subtoken.erase(0, 1); // Remove [
                            uniqueOperands.insert(subtoken);
                        }
                        else if(token.find("[") != std::string::npos && token.find("]") != std::string::npos &&
                        token.find("!") == std::string::npos && token.find(",") == std::string::npos)
                        {   // Third is [r1]
                            subtoken = token;   // Don't ever edit token directly
                            subtoken.erase(subtoken.size() - 1); // Remove ]
                            subtoken.erase(0, 1); // Remove [
                            uniqueOperands.insert(subtoken);
                        }
                        else if(token.find("[") != std::string::npos && token.find("]") != std::string::npos &&
                        token.find("!") == std::string::npos && token.find(",") != std::string::npos)
                        {   // Fourth is [r1],
                            subtoken = token;   // Don't ever edit token directly
                            subtoken.erase(subtoken.size() - 2); // Remove ],
                            subtoken.erase(0, 1); // Remove [
                            uniqueOperands.insert(subtoken);
                        }
                        else if(token.find("[") == std::string::npos && token.find("]") != std::string::npos &&
                        token.find("!") == std::string::npos && token.find("#") == std::string::npos)
                        {   // Fifth is r1]
                            subtoken = token;   // Don't ever edit token directly
                            subtoken.erase(subtoken.size() - 1); // Remove ]
                            uniqueOperands.insert(subtoken);
                        }
                        else if(token.find("[") != std::string::npos && token.find("]") != std::string::npos &&
                        token.find("!") != std::string::npos)
                        {   // Sixth is [r1]!
                            subtoken = token;   // Don't ever edit token directly
                            subtoken.erase(subtoken.size() - 2); // Remove ]!
                            uniqueOperands.insert(subtoken);
                        }
                        else if(token.find("{") != std::string::npos && token.find("}") != std::string::npos)
                        {   // Seventh is {}
                            subtoken = token;   // Don't ever edit token directly
                            subtoken.erase(subtoken.size() - 1); // Remove }
                            subtoken.erase(0, 1); // Remove {
                            uniqueOperands.insert(subtoken);
                        }
                        else if(token.find("{") != std::string::npos && token.find("}") == std::string::npos
                            && token.find(",") != std::string::npos)
                        {   // Eigth is {r1,
                            subtoken = token;   // Don't ever edit token directly
                            subtoken.erase(0, 1); // Remove {
                            uniqueOperands.insert(subtoken);
                        }
                        else if(token.find("{") == std::string::npos && token.find("}") != std::string::npos
                            && token.find(",") == std::string::npos)
                        {   // Ninth is r1}
                            subtoken = token;   // Don't ever edit token directly
                            subtoken.erase(subtoken.size() - 1); // Remove }
                            uniqueOperands.insert(subtoken);
                        }
                        else if(token.find("=") != std::string::npos)
                        {   // Tenth is =variable
                            subtoken = token;   // Don't ever edit token directly
                            subtoken.erase(0, 1); // Remove =
                            uniqueOperands.insert(subtoken);
                        }
                        else if(token.find("#") != std::string::npos && token.find("]") == std::string::npos)
                        {   // Eleventh is literal #
                            subtoken = token;   // Don't ever edit token directly
                            subtoken.erase(0, 1); // Remove #
                            uniqueOperands.insert(subtoken);
                        }
                        else if(token.find("#") != std::string::npos && token.find("]") != std::string::npos)
                        {   // Twelth is literal #]
                            subtoken = token;   // Don't ever edit token directly
                            subtoken.erase(0, 1); // Remove #
                            subtoken.erase(subtoken.size() - 1); // remove ]
                            uniqueOperands.insert(subtoken);
                        }
                        else
                        {   // Last is freestanding, r1  for example
                            uniqueOperands.insert(token);
                            subtoken = token;
                        }

                        /*****************************************************************
                         * The operands following a branch operator. We collect the
                         * branch, where it occurs, and how many branches there are.
                         * We also identify bl and non bl branch use at line.*/
                        if(branchFlag == true)
                        {
                            if(token != "scanf" && token != "printf" && numTokens == 2)
                            {
                                if (blBranchFlag == true)
                                {
                                    subroutineUse.push_back("BL " + token + " at line " + std::to_string(totalLines));
                                    subroutines.insert(token);
                                    blCallLineNum.push_back(totalLines);
                                }
                                else if(bxBranchFlag == true)
                                {
                                    if(token == "lr" || token == "LR")
                                    {
                                        returnLineNum.push_back(totalLines);
                                    }
                                    subroutineUse.push_back("Return branch " + token + " at line " + std::to_string(totalLines));
                                }
                                else
                                {
                                    branchUse.push_back("Branch " + token + " at line " + std::to_string(totalLines));
                                    // badbranches are used to check if subroutines branch outside their bounds
                                    badBranchLineNum.push_back(totalLines);
                                }
                            }
                            else if(token == "scanf" || token == "printf") 
                            {   // Wipe registers on scanf and printf
                                r0Flag = false;
                                r1Flag = false;
                                r2Flag = false;
                                r3Flag = false;
                            }
                        } 
                        /*****************************************************************
                         * If the operator is svc then we check the following operand
                         * for correct exit. We also store svc use at occuring lines */
                        else if(checkSVC == true)
                        {
                            if(token == "0" || token == "#0")
                            {
                                exitExists = true;
                            }
                            svcUse.push_back("SVC " + token + " used at line " + std::to_string(totalLines));
                            checkSVC = false;
                        }
                        /**********************************************************************
                         * We check the token against a set to determine if it is a
                         * register. It is then identified by line and place in instruction.*/
                        else if(registers.find(subtoken) != registers.end())
                        {
                            switch(subtoken[1])
                            {
                                case '0': register0Use.insert(totalLines); break;
                                case '1':   if(subtoken.size() == 2) register1Use.insert(totalLines); 
                                            else if(subtoken[2] == '0') register10Use.insert(totalLines);
                                            else if(subtoken[2] == '1') register11Use.insert(totalLines);
                                            else if(subtoken[2] == '2') register12Use.insert(totalLines);
                                            else if(subtoken[2] == '3') register13Use.insert(totalLines);
                                            else if(subtoken[2] == '4') register14Use.insert(totalLines);
                                            else if(subtoken[2] == '5') register15Use.insert(totalLines);
                                            break;
                                case '2': register2Use.insert(totalLines); break;
                                case '3': register3Use.insert(totalLines); break;
                                case '4': register4Use.insert(totalLines); break;
                                case '5': register5Use.insert(totalLines); break;
                                case '6': register6Use.insert(totalLines); break;
                                case '7': register7Use.insert(totalLines); break;
                                case '8': register8Use.insert(totalLines); break;
                                case '9': register9Use.insert(totalLines); break;
                            }

                            // If the token is a register and the first operand then it is being loaded with a value
                            // ignore use of CMP since that doesn't load into the first operand
                            if(numTokens == 2 && cmpNextLine == false && strFlag == false)
                            {
                                // Set a flag saying the register has been loaded if it is the second token, aka first operand
                                switch(subtoken[1])
                                {
                                    case '0': r0Flag = true; break;
                                    case '1':   if(subtoken.size() == 2) r1Flag = true; 
                                                else if(subtoken[2] == '0') r10Flag = true;
                                                else if(subtoken[2] == '1') r11Flag = true;
                                                else if(subtoken[2] == '2') r12Flag = true;
                                                break;
                                    case '2': r2Flag = true; break;
                                    case '3': r3Flag = true; break;
                                    case '4': r4Flag = true; break;
                                    case '5': r5Flag = true; break;
                                    case '6': r6Flag = true; break;
                                    case '7': r7Flag = true; break;
                                    case '8': r8Flag = true; break;
                                    case '9': r9Flag = true; break;
                                }
                            }   // If the operator was a POP then everything that comes after it is loaded with a value
                            else if(popFlag == true)
                            {
                                switch(subtoken[1])
                                {
                                    case '0': r0Flag = true; break;
                                    case '1':   if(subtoken.size() == 2) r1Flag = true; 
                                                else if(subtoken[2] == '0') r10Flag = true;
                                                else if(subtoken[2] == '1') r11Flag = true;
                                                else if(subtoken[2] == '2') r12Flag = true;
                                                break;
                                    case '2': r2Flag = true; break;
                                    case '3': r3Flag = true; break;
                                    case '4': r4Flag = true; break;
                                    case '5': r5Flag = true; break;
                                    case '6': r6Flag = true; break;
                                    case '7': r7Flag = true; break;
                                    case '8': r8Flag = true; break;
                                    case '9': r9Flag = true; break;
                                }
                            }
                            else if(strFlag == true && numTokens == 2)
                            {
                                // The format is to check if the register has been used, then see if it has been loaded
                                // and to toss an error if it hasn't
                                // Error flags are to prevent duplicate errors being thrown for repeated register use
                                // on the same line
                                switch(subtoken[1])
                                {
                                    case '0':   if(r0Flag == false && r0ErrorFlag == false)
                                                {
                                                    r0ErrorFlag = true;
                                                    registerError.push_back("Register 0 used before being loaded at line "
                                                    + std::to_string(totalLines));
                                                } 
                                                break;
                                    case '1':   if(subtoken.size() == 2)
                                                {
                                                    if(r1Flag == false && r1ErrorFlag == false)
                                                    {
                                                        r1ErrorFlag = true;
                                                        registerError.push_back("Register 1 used before being loaded at line " 
                                                        + std::to_string(totalLines));
                                                    }
                                                } 
                                                else if(subtoken[2] == '0')
                                                {
                                                    if(r10Flag == false && r10ErrorFlag == false)
                                                    {
                                                        r10ErrorFlag = true;
                                                        registerError.push_back("Register 10 used before being loaded at line " 
                                                        + std::to_string(totalLines));
                                                    }
                                                } 
                                                else if(subtoken[2] == '1')
                                                {
                                                    if(r11Flag == false && r11ErrorFlag == false)
                                                    {
                                                        r11ErrorFlag = true;
                                                        registerError.push_back("Register 11 used before being loaded at line " 
                                                        + std::to_string(totalLines));
                                                    }
                                                } 
                                                else if(subtoken[2] == '2')
                                                {
                                                    if(r12Flag == false && r12ErrorFlag == false)
                                                    {
                                                        r12ErrorFlag = true;
                                                        registerError.push_back("Register 12 used before being loaded at line " 
                                                        + std::to_string(totalLines));
                                                    }
                                                } 
                                                break;
                                    case '2':   if(r2Flag == false && r2ErrorFlag == false)
                                                {
                                                    r2ErrorFlag = true;
                                                    registerError.push_back("Register 2 used before being loaded at line "
                                                    + std::to_string(totalLines));
                                                } 
                                                break;
                                    case '3':   if(r3Flag == false && r3ErrorFlag == false)
                                                {
                                                    r3ErrorFlag = true;
                                                    registerError.push_back("Register 3 used before being loaded at line "
                                                    + std::to_string(totalLines));
                                                } 
                                                break;
                                    case '4':   if(r4Flag == false && r4ErrorFlag == false)
                                                {
                                                    r4ErrorFlag = true;
                                                    registerError.push_back("Register 4 used before being loaded at line "
                                                    + std::to_string(totalLines));
                                                } 
                                                break;
                                    case '5':   if(r5Flag == false && r5ErrorFlag == false)
                                                {
                                                    r5ErrorFlag = true;
                                                    registerError.push_back("Register 5 used before being loaded at line "
                                                    + std::to_string(totalLines));
                                                } 
                                                break;
                                    case '6':   if(r6Flag == false && r6ErrorFlag == false)
                                                {
                                                    r6ErrorFlag = true;
                                                    registerError.push_back("Register 6 used before being loaded at line "
                                                    + std::to_string(totalLines));
                                                } 
                                                break;
                                    case '7':   if(r7Flag == false && r7ErrorFlag == false)
                                                {
                                                    r7ErrorFlag = true;
                                                    registerError.push_back("Register 7 used before being loaded at line "
                                                    + std::to_string(totalLines));
                                                } 
                                                break;
                                    case '8':   if(r8Flag == false && r8ErrorFlag == false)
                                                {
                                                    r8ErrorFlag = true;
                                                    registerError.push_back("Register 8 used before being loaded at line "
                                                    + std::to_string(totalLines));
                                                } 
                                                break;
                                    case '9':   if(r9Flag == false && r9ErrorFlag == false)
                                                {
                                                    r9ErrorFlag = true;
                                                    registerError.push_back("Register 9 used before being loaded at line "
                                                    + std::to_string(totalLines));
                                                } 
                                                break;
                                }
                            }
                            else if(numTokens > 2)   // The token is any other operand then the first, and is a register
                            {
                                // The format is to check if the register has been used, then see if it has been loaded
                                // and to toss an error if it hasn't
                                switch(subtoken[1])
                                {
                                    case '0':   if(r0Flag == false && r0ErrorFlag == false)
                                                {
                                                    r0ErrorFlag = true;
                                                    registerError.push_back("Register 0 used before being loaded at line "
                                                    + std::to_string(totalLines));
                                                } 
                                                break;
                                    case '1':   if(subtoken.size() == 2)
                                                {
                                                    if(r1Flag == false && r1ErrorFlag == false)
                                                    {
                                                        r1ErrorFlag = true;
                                                        registerError.push_back("Register 1 used before being loaded at line " 
                                                        + std::to_string(totalLines));
                                                    }
                                                } 
                                                else if(subtoken[2] == '0')
                                                {
                                                    if(r10Flag == false && r10ErrorFlag == false)
                                                    {
                                                        r10ErrorFlag = true;
                                                        registerError.push_back("Register 10 used before being loaded at line " 
                                                        + std::to_string(totalLines));
                                                    }
                                                } 
                                                else if(subtoken[2] == '1')
                                                {
                                                    if(r11Flag == false && r11ErrorFlag == false)
                                                    {
                                                        r11ErrorFlag = true;
                                                        registerError.push_back("Register 11 used before being loaded at line " 
                                                        + std::to_string(totalLines));
                                                    }
                                                } 
                                                else if(subtoken[2] == '2')
                                                {
                                                    if(r12Flag == false && r12ErrorFlag == false)
                                                    {
                                                        r12ErrorFlag = true;
                                                        registerError.push_back("Register 12 used before being loaded at line " 
                                                        + std::to_string(totalLines));
                                                    }
                                                } 
                                                break;
                                    case '2':   if(r2Flag == false && r2ErrorFlag == false)
                                                {
                                                    r2ErrorFlag = true;
                                                    registerError.push_back("Register 2 used before being loaded at line "
                                                    + std::to_string(totalLines));
                                                } 
                                                break;
                                    case '3':   if(r3Flag == false && r3ErrorFlag == false)
                                                {
                                                    r3ErrorFlag = true;
                                                    registerError.push_back("Register 3 used before being loaded at line "
                                                    + std::to_string(totalLines));
                                                } 
                                                break;
                                    case '4':   if(r4Flag == false && r4ErrorFlag == false)
                                                {
                                                    r4ErrorFlag = true;
                                                    registerError.push_back("Register 4 used before being loaded at line "
                                                    + std::to_string(totalLines));
                                                } 
                                                break;
                                    case '5':   if(r5Flag == false && r5ErrorFlag == false)
                                                {
                                                    r5ErrorFlag = true;
                                                    registerError.push_back("Register 5 used before being loaded at line "
                                                    + std::to_string(totalLines));
                                                } 
                                                break;
                                    case '6':   if(r6Flag == false && r6ErrorFlag == false)
                                                {
                                                    r6ErrorFlag = true;
                                                    registerError.push_back("Register 6 used before being loaded at line "
                                                    + std::to_string(totalLines));
                                                } 
                                                break;
                                    case '7':   if(r7Flag == false && r7ErrorFlag == false)
                                                {
                                                    r7ErrorFlag = true;
                                                    registerError.push_back("Register 7 used before being loaded at line "
                                                    + std::to_string(totalLines));
                                                } 
                                                break;
                                    case '8':   if(r8Flag == false && r8ErrorFlag == false)
                                                {
                                                    r8ErrorFlag = true;
                                                    registerError.push_back("Register 8 used before being loaded at line "
                                                    + std::to_string(totalLines));
                                                } 
                                                break;
                                    case '9':   if(r9Flag == false && r9ErrorFlag == false)
                                                {
                                                    r9ErrorFlag = true;
                                                    registerError.push_back("Register 9 used before being loaded at line "
                                                    + std::to_string(totalLines));
                                                } 
                                                break;
                                }
                            }
                        }
                        /***************************************************************
                         * If the operator was PUSH we want to check if the LR is saved
                         * for future checks            */
                        else if(pushFlag == true)
                        {
                            if(token == "{lr}" || token == "{LR}")
                            {
                                lrSaveLineNum.push_back(totalLines);
                            }
                        }
                        /***************************************************************
                         * If movflag is true, then we check to see if LR was saved or
                         * check for mov pc, lr for a way to return 
                         * needs to be seperate if statement to not conflict 
                         * with checking for restricted registers*/ 
                        if(movFlag == true)
                        {   
                            if(movPCFlag == true)
                            {
                                if (token == "lr" || token == "LR")
                                {
                                    returnLineNum.push_back(totalLines);
                                }
                            }
                            if((token == "lr" || token == "LR") && numTokens == 3)
                            { // mov r, lr is a save format
                                lrSaveLineNum.push_back(totalLines);
                            }
                            else if (token == "pc," || token == "PC,")
                            {
                                movPCFlag = true; // check for mov pc, lr
                            }
                            /*****************************************************************
                             * If ldr or mov was used then we need to validate that the 
                             * registers r13, r14, and r15 were not used. We validate against
                             * a set.*/
                            if(restrictedRegisterFlag == true)
                            {
                                if(restrictedRegisters.find(subtoken) != restrictedRegisters.end())
                                {
                                    restrictedError.push_back("Improper use of restricted register "
                                    + subtoken + " at line " + std::to_string(totalLines));
                                }
                            }
                        }
                    }
                    /********************************************************************
                     * A token is a directive if it begins with a . and is followed
                     * by a letter. Directives provide instructions to how the code
                     * should be handled.*/
                    else if (token[0] == '.' && std::isalpha(token[1]))
                    {
                        dirLines++;
                        directiveUse[token].push_back(totalLines);
                        //directiveUse.push_back(token + " directive used at line " + std::to_string(totalLines));

                        /*****************************************************************
                         * .global is checked for to see if it comes before .data*/
                        if (token == ".global") 
                        {
                            globalFlag = true; // We have seen global directive
                            dataFlag = false;
                        }
                        /*****************************************************************
                         * .data is checked first to see if it comes before .global
                         * and also to determine user defined variables*/
                        else if (token == ".data")
                        {
                            dataFlag = true; // Inside the .data section
                            dataExists = true; // .data is inside file
                            dataLineNum = totalLines;
                            if (globalFlag == false)    // If .data comes before .global
                            {
                                globalErrorFlag = true;
                            }
                        }
                        /*****************************************************************
                         * .equ will be checked so that the following tokens will be 
                         * handled to validate the use of constants*/
                        else if (token == ".equ")
                        {
                            equFlag = true;
                        }
                        /*****************************************************************
                         * If we are in .data section and the token is .global, handled 
                         * above, or .text, then we are out of the .data section.
                         * These are the two most common exits and anything more 
                         * complicated than that will be very visually wrong. */
                        if(token == ".text")
                        {
                            dataFlag = false; // Out of .data section
                        }
                    }
                    /********************************************************************
                     * If we are in the .data section, and the token ends with a :
                     * then the token is defining a user defined variable */
                    else if(token.back() == ':' && dataFlag == true && numTokens == 1)
                    {
                        subtoken = token;   // Never edit the token
                        subtoken.erase(subtoken.size() - 1);    // Cut off the :
                        variables.push_back(subtoken);
                    }
                    /********************************************************************
                     * If the token is not in the .data section and ends in a :
                     * then it is a label defining a section of the program */
                    else if(token.back() == ':' && dataFlag == false && numTokens == 1)
                    {
                        numTokens--;
                        subtoken = token; // Never edit the token
                        subtoken.erase(subtoken.size() - 1);    // Cut off the :
                        labels.push_back(subtoken);
                        labelLineNum.push_back(totalLines); // The line the label starts at
                        noReturnBranch = false; // Once a label is found code can be reached again
                    }
                    /********************************************************************
                     * If we are in the .equ section
                     * then the second token in the line is a constant */
                    else if(equFlag == true && numTokens == 2)
                    {
                        subtoken = token; // Never edit the token
                        subtoken.erase(subtoken.size() - 1); // Cut of the ,
                        constants.push_back(subtoken);
                    }

                    if(pushFlag == true && numTokens != 1)
                    {
                        pushNum++;
                    }
                    else if(popFlag == true && numTokens != 1)
                    {
                        popNum++;
                    }
                }
            }
        }

        /*******************************************************************
         *              START LINE BASED CHECKS HERE        */

        /*******************************************************************
         * Addressing modes, moved to lines to deal with bad flagging.
         * Ordered by simplicity of check.*/
        if(ldrFlag == true || strFlag == true)
        {
            if (linePreComment.find("=") != std::string::npos)
            {
                pcLiteralMode.push_back(std::to_string(totalLines));
            }
            else if(numTokens == 3) 
            {
                indirectMode.push_back(std::to_string(totalLines));
            }
            else if(linePreComment.find("!") != std::string::npos)
            {
                preIndexMode.push_back(std::to_string(totalLines));
            }
            else if(linePreComment.find("PC") != std::string::npos || linePreComment.find("pc") != std::string::npos) 
            {
                pcRelativeMode.push_back(std::to_string(totalLines));
            }
            else if(numTokens == 4 && token.back() == ']')
            {
                indirectOffsetMode.push_back(std::to_string(totalLines));
            }
            else if(numTokens == 4 && token.back() != ']' && token.back() != '!')
            {
                postIndexMode.push_back(std::to_string(totalLines));
            }
            else
                unsureMode.push_back(std::to_string(totalLines));
        }
        
        /********************************************************************
         * If the token is within the .data section. We validate the line has
         * a quote to define a string, then check if it ends in \ n 
         * We seperate this section from others because it works with the
         * whole line.*/
        if(dataFlag == true)
        {  
            // Ignore common .data elements that would flag like numInputPattern:
            if(line.find("numInputPattern:") == std::string::npos &&
            line.find("strInputPattern:") == std::string::npos &&
            line.find("strInputError:") == std::string::npos)
            {   // Check that the line has a quote but doesn't end a quote with \n"
                if(line.find('"') != std::string::npos && line.find("\\n\"") == std::string::npos) 
                {
                    stringError.push_back("String did not end with \\n at line " + std::to_string(totalLines));
                }
            }
        }
    }

    /*******************************************************************************
     * A value that is defined would be used as an operand somewhere in the program.
     * If the value is not in the list of unique operands then it was not used. */
    for(size_t i = 0; i < labels.size(); i++)
    {
        if(uniqueOperands.find(labels[i]) == uniqueOperands.end())
        {
            unusedLabel.push_back("Unused label: " + labels[i]);
        }
    }
    for(size_t i = 0; i < variables.size(); i++)
    {
        if(uniqueOperands.find(variables[i]) == uniqueOperands.end())
        {
            unusedVariable.push_back("Unused user variable: " + variables[i]);
        }
    }
    for(size_t i = 0; i < constants.size(); i++)
    {
        if(uniqueOperands.find(constants[i]) == uniqueOperands.end())
        {
            unusedConstant.push_back("Unused user constant: " + constants[i]);
        }
    }

    /*****************************************************************************
     * Label analyzer, it first defines what kind of label is being checked first
     * then reads in line numbers of various flags to determine what happens 
     * within said label to determine errors. It works because label and
     * labelLineNum are correlated positionally. */
    for(size_t i = 0; i < labels.size(); i++)
    {
        subroutineFlag = false;
        returnFlag = false;
        subroutineCall = false;
        lrSaved = false;

        // Declare whether the current label is a subroutine
        if(subroutines.find(labels[i]) != subroutines.end()) subroutineFlag = true;

        if(subroutineFlag == true)  
        {
            // The last label will have nothing to compare to, so we use .data instead
            if(i + 1 != labels.size())
            {
                nextLabel = labelLineNum[i+1];
            }
            else if(i + 1 == labels.size())
            {
                nextLabel = dataLineNum;
            }
            // If the label is a subroutine, check that it has a return before the label ends
            for (size_t j = 0; j < returnLineNum.size(); j++)
            {
                // If any of the returns happen between when the label starts and ends
                if(returnLineNum[j] >= labelLineNum[i] && returnLineNum[j] < nextLabel)
                {
                    returnFlag = true;
                }
            }
            // Check if a call to a subroutine is made inside the subroutine
            for (size_t j = 0; j < blCallLineNum.size(); j++)
            {   // If any of the subroutine calls happen between label start and end
                if(blCallLineNum[j] >= labelLineNum[i] && blCallLineNum[j] < nextLabel)
                {
                    subroutineCall = true;
                    for(size_t k = 0; k < lrSaveLineNum.size(); k++)
                    {   // If the LR was saved in a label where a subroutine was called after the label line
                        // number but before the subroutine call line number
                        if(lrSaveLineNum[k] >= labelLineNum[i] && lrSaveLineNum[k] <= blCallLineNum[j])
                        {
                            lrSaved = true;
                        }
                    }
                }
            }
            // Check if  subroutine branches outside of its bounds
            for (size_t j = 0; j < badBranchLineNum.size(); j++)
            {
                // If any of the returns happen between when the label starts and ends
                if(badBranchLineNum[j] >= labelLineNum[i] && badBranchLineNum[j] < nextLabel)
                {
                    branchOutError.push_back(labels[i] + " branches out of the subroutine bounds at line " + 
                    std::to_string(badBranchLineNum[j]));
                }
            }
        }
        // If there is a subroutine but not a return
        if(subroutineFlag == true && returnFlag == false)
        {
            noReturnError.push_back(labels[i] + " has no return despite being a subroutine.");
        }
        // If there is a subroutine but not a saved spot
        if(subroutineCall == true && lrSaved == false)
        {
            lrSaveError.push_back(labels[i] + " has a call to a subroutine in it without saving the LR first.");
        }
    }

    // This section is where you add logic to determine or calculate metrics/errors
    // Halstead's
    int length = totalOperators + totalOperands; 
    int vocabulary = uniqueOperators.size() + uniqueOperands.size();
    double volume = length * log2(vocabulary);
    double difficulty = (double(uniqueOperators.size()) / 2.0) * (double(totalOperands) / double(uniqueOperands.size())); 
    double effort = difficulty * volume; 

    /***********************************************
     * Meta data        */
    std::string TOOL_VERSION = "1.0";
    std::string TOOL_DATE = "4/27/2024";
    struct stat file_stat;
    std::time_t access_time;
    std::time_t mod_time;

    stat(input_file.c_str(), &file_stat);
    access_time = file_stat.st_atime;
    mod_time = file_stat.st_mtime;

    namespace fs = std::filesystem;
    fs::path filePath(input_file);
    std::string fileName = filePath.filename().string();


    // Case 1 = Metrics to terminal, Case 2 = Errors to terminal
    // Case 3 = Make report file
    switch(command) 
    {
        case 4:
            /**********************************************************************************
             * If the csv file doesn't exist, create the header row, otherwise
             * we just want to append new data 
             * Have to use put_time and local time to avoid ctimes newline character ending*/
            if(!std::filesystem::exists(output_file))
            {
                std::ofstream writecsv(output_file, std::ios::app);
                writecsv << "File name, Last Accessed, Last Modified, Halstead's Total Operators,"
                            << " Total Operands, Unique Operators, Unique Operands, Length, Vocabulary, Volume, Difficulty,"
                            << " Effort\n";
                writecsv << fileName << ", " << std::put_time(std::localtime(&access_time), "%c") << ", "
                << std::put_time(std::localtime(&mod_time), "%c") << ", " << totalOperators << ", " << totalOperands 
                << ", " << uniqueOperators.size() << ", " << uniqueOperands.size() << ", " << length << ", " << vocabulary 
                << ", " << volume << ", " << difficulty << ", " << effort << "\n";
            }
            else
            {
                std::ofstream writecsv(output_file, std::ios::app);
                writecsv << fileName << ", " << std::put_time(std::localtime(&access_time), "%c") << ", "
                << std::put_time(std::localtime(&mod_time), "%c") << ", " << totalOperators << ", " << totalOperands 
                << ", " << uniqueOperators.size() << ", " << uniqueOperands.size() << ", " << length << ", " << vocabulary 
                << ", " << volume << ", " << difficulty << ", " << effort << "\n";
            }
            break;

        case 1:
            std::cout << "********************************************************\nMetadata:\n";
            std::cout << "\tFile Name: " << fileName << "\n";
            std::cout << "\tLast accessed: " << std::ctime(&access_time);
            std::cout << "\tLast modified: " << std::ctime(&mod_time);
            std::cout << "\tTool Version: " << TOOL_VERSION << "\n";
            std::cout << "\tTool Date: " << TOOL_DATE << "\n";
            std::cout << "********************************************************\nGeneral Metrics:\n";
            std::cout << "\tNumber of full line comments: " << fullCommentLines << "\n";
            std::cout << "\tNumber of blank lines: " << blankLines << "\n";
            std::cout << "\tTotal number of lines: " << totalLines << "\n";
            std::cout << "\tNumber of lines with comments: " << linesWComment << "\n";
            std::cout << "\tNumber of lines without comments: " << linesWOComment << "\n";
            std::cout << "\tTotal directives used: " << dirLines << "\n";
            std::cout << "\tCyclomatic Complexity: " << cyclomatic << "\n";
            std::cout << "********************************************************\n";
            std::cout << "Halstead's Metrics:\n";
            std::cout << "\tUnique operators: " << uniqueOperators.size() << "\n";
            std::cout << "\tTotal operators: " << totalOperators << "\n";
            std::cout << "\tUnique operands: " << uniqueOperands.size() << "\n";
            std::cout << "\tTotal operands: " << totalOperands << "\n";
            std::cout << "\tProgram Length: " << length << "\n";
            std::cout << "\tProgram Vocabulary: " << vocabulary << "\n";
            std::cout << "\tProgram Volume: " << volume << "\n";
            std::cout << "\tProgram Difficulty: " << difficulty << "\n";
            std::cout << "\tProgram Effort: " << effort << "\n";
            std::cout << "********************************************************\n";
            std::cout << "Register Use:\n";
            std::cout << "\tRegister 0 used at lines: ";
            sorter.assign(register0Use.begin(), register0Use.end());
            std::sort(sorter.begin(), sorter.end());    // Sort register use
            for(auto& line : sorter)
            {
                std::cout << line << " ";
            }
            std::cout << "\n\tRegister 1 used at lines: ";
            sorter.clear();     // Sorter ready for reuse
            sorter.assign(register1Use.begin(), register1Use.end());
            std::sort(sorter.begin(), sorter.end());    // Sort register use
            for(auto& line : sorter)
            {
                std::cout << line << " ";
            }
            std::cout << "\n\tRegister 2 used at lines: ";
            sorter.clear();     // Sorter ready for reuse
            sorter.assign(register2Use.begin(), register2Use.end());
            std::sort(sorter.begin(), sorter.end());    // Sort register use
            for(auto& line : sorter)
            {
                std::cout << line << " ";
            }
            std::cout << "\n\tRegister 3 used at lines: ";
            sorter.clear();     // Sorter ready for reuse
            sorter.assign(register3Use.begin(), register3Use.end());
            std::sort(sorter.begin(), sorter.end());    // Sort register use
            for(auto& line : sorter)
            {
                std::cout << line << " ";
            }
            std::cout << "\n\tRegister 4 used at lines: ";
            sorter.clear();     // Sorter ready for reuse
            sorter.assign(register4Use.begin(), register4Use.end());
            std::sort(sorter.begin(), sorter.end());    // Sort register use
            for(auto& line : sorter)
            {
                std::cout << line << " ";
            }
            std::cout << "\n\tRegister 5 used at lines: ";
            sorter.clear();     // Sorter ready for reuse
            sorter.assign(register5Use.begin(), register5Use.end());
            std::sort(sorter.begin(), sorter.end());    // Sort register use
            for(auto& line : sorter)
            {
                std::cout << line << " ";
            }
            std::cout << "\n\tRegister 6 used at lines: ";
            sorter.clear();     // Sorter ready for reuse
            sorter.assign(register6Use.begin(), register6Use.end());
            std::sort(sorter.begin(), sorter.end());    // Sort register use
            for(auto& line : sorter)
            {
                std::cout << line << " ";
            }
            std::cout << "\n\tRegister 7 used at lines: ";
            sorter.clear();     // Sorter ready for reuse
            sorter.assign(register7Use.begin(), register7Use.end());
            std::sort(sorter.begin(), sorter.end());    // Sort register use
            for(auto& line : sorter)
            {
                std::cout << line << " ";
            }
            std::cout << "\n\tRegister 8 used at lines: ";
            sorter.clear();     // Sorter ready for reuse
            sorter.assign(register8Use.begin(), register8Use.end());
            std::sort(sorter.begin(), sorter.end());    // Sort register use
            for(auto& line : sorter)
            {
                std::cout << line << " ";
            }
            std::cout << "\n\tRegister 9 used at lines: ";
            sorter.clear();     // Sorter ready for reuse
            sorter.assign(register9Use.begin(), register9Use.end());
            std::sort(sorter.begin(), sorter.end());    // Sort register use
            for(auto& line : sorter)
            {
                std::cout << line << " ";
            }
            std::cout << "\n\tRegister 10 used at lines: ";
            sorter.clear();     // Sorter ready for reuse
            sorter.assign(register10Use.begin(), register10Use.end());
            std::sort(sorter.begin(), sorter.end());    // Sort register use
            for(auto& line : sorter)
            {
                std::cout << line << " ";
            }
            std::cout << "\n\tRegister 11 used at lines: ";
            sorter.clear();     // Sorter ready for reuse
            sorter.assign(register11Use.begin(), register11Use.end());
            std::sort(sorter.begin(), sorter.end());    // Sort register use
            for(auto& line : sorter)
            {
                std::cout << line << " ";
            }
            std::cout << "\n\tRegister 12 used at lines: ";
            sorter.clear();     // Sorter ready for reuse
            sorter.assign(register12Use.begin(), register12Use.end());
            std::sort(sorter.begin(), sorter.end());    // Sort register use
            for(auto& line : sorter)
            {
                std::cout << line << " ";
            }
            std::cout << "\n\tRegister 13 used at lines: ";
            sorter.clear();     // Sorter ready for reuse
            sorter.assign(register13Use.begin(), register13Use.end());
            std::sort(sorter.begin(), sorter.end());    // Sort register use
            for(auto& line : sorter)
            {
                std::cout << line << " ";
            }
            std::cout << "\n\tRegister 14 used at lines: ";
            sorter.clear();     // Sorter ready for reuse
            sorter.assign(register14Use.begin(), register14Use.end());
            std::sort(sorter.begin(), sorter.end());    // Sort register use
            for(auto& line : sorter)
            {
                std::cout << line << " ";
            }
            std::cout << "\n\tRegister 15 used at lines: ";
            sorter.clear();     // Sorter ready for reuse
            sorter.assign(register15Use.begin(), register15Use.end());
            std::sort(sorter.begin(), sorter.end());    // Sort register use
            for(auto& line : sorter)
            {
                std::cout << line << " ";
            }
            std::cout << "\n********************************************************\n";
            std::cout << "SVC Use:\n";
            for(auto& line : svcUse)
            {
                std::cout << "\t" << line << "\n";
            }
            std::cout << "Subroutine Use:\n";
            for(auto& line : subroutineUse)
            {
                std::cout << "\t" << line << "\n";
            }
            std::cout << "Branch Use:\n";
            for(auto& line : branchUse)
            {
                std::cout << "\t" << line << "\n";
            }
            std::cout << "Directive Use:\n";
            for (auto& map : directiveUse) 
            {
                std::cout << "\t" << map.first << " at lines: ";
                for (size_t i = 0; i < map.second.size(); i++) 
                {
                    std::cout << map.second[i] << " ";
                }
                std::cout << "\n";
            }
            std::cout << "********************************************************\n";
            std::cout << "Addressing Modes:\n";
            std::cout << "\tLines with indirect addressing: ";
            for(auto& line : indirectMode)
            {
                std::cout << line << " ";
            }
            std::cout << "\n\tLines with indirect addressing with offset: ";
            for(auto& line : indirectOffsetMode)
            {
                std::cout << line << " ";
            }
            std::cout << "\n\tLines with auto, pre-index addressing: ";
            for(auto& line : preIndexMode)
            {
                std::cout << line << " ";
            }
            std::cout << "\n\tLines with auto, post-index addressing: ";
            for(auto& line : postIndexMode)
            {
                std::cout << line << " ";
            }
            std::cout << "\n\tLines with PC relative addressing: ";
            for(auto& line : pcRelativeMode)
            {
                std::cout << line << " ";
            }
            std::cout << "\n\tLines with PC relative addressing with literal pool: ";
            for(auto& line : pcLiteralMode)
            {
                std::cout << line << " ";
            }
            std::cout << "\n\tLines with uncertain addressing modes: ";
            for(auto& line : unsureMode)
            {
                std::cout << line << " ";
            }
            std::cout << "\n********************************************************\n";
            break;

        case 2:
            if (dataExists == false)
            {
                std::cout << fileName << ": Catastrophic error: Missing .data section. Error must be addressed before using AEC" << "\n";
            }
            else if (globalErrorFlag == true)
            {
                std::cout << fileName << ": Catastrophic error: .data section comes before .global. Error must be addressed before using AEC" << "\n";
            }
            else
            {
                std::cout << "********************************************************\nMetadata:\n";
                std::cout << "\tFile Name: " << fileName << "\n";
                std::cout << "\tLast accessed: " << std::ctime(&access_time);
                std::cout << "\tLast modified: " << std::ctime(&mod_time);
                std::cout << "\tTool Version: " << TOOL_VERSION << "\n";
                std::cout << "\tTool Date: " << TOOL_DATE << "\n";
                std::cout << "********************************************************\nErrors found:\n";
            
                if (exitExists == false)
                {
                    std::cout << "\tNo proper exit, svc 0, from program before .data section\n";
                }
                if(pushNum > popNum)
                {
                    std::cout << "\tMore pushes detected than pops. Ensure that all values are popped off the heap.\n";
                }
                else if(pushNum < popNum)
                {
                    std::cout << "\tMore pops detected than pushes. Ensure that there is always a value on the heap before a Pop.\n";
                }

                for(auto& line : stringError)
                {
                    std::cout << "\t" << line << "\n";
                }
                for(auto& line : unwantedInstructions)
                {
                    std::cout << "\t" << line << "\n";
                }
                for(auto& line : restrictedError)
                {
                    std::cout << "\t" << line << "\n";
                }
                for(auto& line : unusedConditional)
                {
                    std::cout << "\t" << line << "\n";
                }
                for(auto& line : unusedLabel)
                {
                    std::cout << "\t" << line << "\n";
                }
                for(auto& line : unusedVariable)
                {
                    std::cout << "\t" << line << "\n";
                }
                for(auto& line : unusedConstant)
                {
                    std::cout << "\t" << line << "\n";
                }
                for(auto& line : isolatedCode)
                {
                    std::cout << "\t" << line << "\n";
                }
                for(auto& line : noReturnError)
                {
                    std::cout << "\t" << line << "\n";
                }
                for(auto& line : lrSaveError)
                {
                    std::cout << "\t" << line << "\n";
                }
                for(auto& line : branchOutError)
                {
                    std::cout << "\t" << line << "\n";
                }
                for(auto& line : registerError)
                {
                    std::cout << "\t" << line << "\n";
                }
                std::cout << "********************************************************\n";
            
            }
            break;

        case 3:
            if (dataExists == false)
            {
                std::cout << fileName << ": Catastrophic error: Missing .data section. Error must be addressed before using AEC" << "\n";
            }
            else if (globalErrorFlag == true)
            {
                std::cout << fileName << ": Catastrophic error: .data section comes before .global. Error must be addressed before using AEC" << "\n";
            }
            else
            {
                std::ofstream outfile(output_file);
                outfile << "********************************************************\nMetadata:\n";
                outfile << "\tFile Name: " << fileName << "\n";
                outfile << "\tLast accessed: " << std::ctime(&access_time);
                outfile << "\tLast modified: " << std::ctime(&mod_time);
                outfile << "\tTool Version: " << TOOL_VERSION << "\n";
                outfile << "\tTool Date: " << TOOL_DATE << "\n";
                outfile << "********************************************************\nGeneral Metrics:\n";
                outfile << "\tNumber of full line comments: " << fullCommentLines << "\n";
                outfile << "\tNumber of blank lines: " << blankLines << "\n";
                outfile << "\tTotal number of lines: " << totalLines << "\n";
                outfile << "\tNumber of lines with comments: " << linesWComment << "\n";
                outfile << "\tNumber of lines without comments: " << linesWOComment << "\n";
                outfile << "\tTotal directives used: " << dirLines << "\n";
                outfile << "\tCyclomatic Complexity: " << cyclomatic << "\n";
                outfile << "********************************************************\n";
                outfile << "Halstead's Metrics:\n";
                outfile << "\tUnique operators: " << uniqueOperators.size() << "\n";
                outfile << "\tTotal operators: " << totalOperators << "\n";
                outfile << "\tUnique operands: " << uniqueOperands.size() << "\n";
                outfile << "\tTotal operands: " << totalOperands << "\n";
                outfile << "\tProgram Length: " << length << "\n";
                outfile << "\tProgram Vocabulary: " << vocabulary << "\n";
                outfile << "\tProgram Volume: " << volume << "\n";
                outfile << "\tProgram Difficulty: " << difficulty << "\n";
                outfile << "\tProgram Effort: " << effort << "\n";
                outfile << "********************************************************\n";
                outfile << "Register Use:\n";
                outfile << "\tRegister 0 used at lines: ";
                sorter.assign(register0Use.begin(), register0Use.end());
                std::sort(sorter.begin(), sorter.end());    // Sort register use
                for(auto& line : sorter)
                {
                    outfile << line << " ";
                }
                outfile << "\n\tRegister 1 used at lines: ";
                sorter.clear();     // Sorter ready for reuse
                sorter.assign(register1Use.begin(), register1Use.end());
                std::sort(sorter.begin(), sorter.end());    // Sort register use
                for(auto& line : sorter)
                {
                    outfile << line << " ";
                }
                outfile << "\n\tRegister 2 used at lines: ";
                sorter.clear();     // Sorter ready for reuse
                sorter.assign(register2Use.begin(), register2Use.end());
                std::sort(sorter.begin(), sorter.end());    // Sort register use
                for(auto& line : sorter)
                {
                    outfile << line << " ";
                }
                outfile << "\n\tRegister 3 used at lines: ";
                sorter.clear();     // Sorter ready for reuse
                sorter.assign(register3Use.begin(), register3Use.end());
                std::sort(sorter.begin(), sorter.end());    // Sort register use
                for(auto& line : sorter)
                {
                    outfile << line << " ";
                }
                outfile << "\n\tRegister 4 used at lines: ";
                sorter.clear();     // Sorter ready for reuse
                sorter.assign(register4Use.begin(), register4Use.end());
                std::sort(sorter.begin(), sorter.end());    // Sort register use
                for(auto& line : sorter)
                {
                    outfile << line << " ";
                }
                outfile << "\n\tRegister 5 used at lines: ";
                sorter.clear();     // Sorter ready for reuse
                sorter.assign(register5Use.begin(), register5Use.end());
                std::sort(sorter.begin(), sorter.end());    // Sort register use
                for(auto& line : sorter)
                {
                    outfile << line << " ";
                }
                outfile << "\n\tRegister 6 used at lines: ";
                sorter.clear();     // Sorter ready for reuse
                sorter.assign(register6Use.begin(), register6Use.end());
                std::sort(sorter.begin(), sorter.end());    // Sort register use
                for(auto& line : sorter)
                {
                    outfile << line << " ";
                }
                outfile << "\n\tRegister 7 used at lines: ";
                sorter.clear();     // Sorter ready for reuse
                sorter.assign(register7Use.begin(), register7Use.end());
                std::sort(sorter.begin(), sorter.end());    // Sort register use
                for(auto& line : sorter)
                {
                    outfile << line << " ";
                }
                outfile << "\n\tRegister 8 used at lines: ";
                sorter.clear();     // Sorter ready for reuse
                sorter.assign(register8Use.begin(), register8Use.end());
                std::sort(sorter.begin(), sorter.end());    // Sort register use
                for(auto& line : sorter)
                {
                    outfile << line << " ";
                }
                outfile << "\n\tRegister 9 used at lines: ";
                sorter.clear();     // Sorter ready for reuse
                sorter.assign(register9Use.begin(), register9Use.end());
                std::sort(sorter.begin(), sorter.end());    // Sort register use
                for(auto& line : sorter)
                {
                    outfile << line << " ";
                }
                outfile << "\n\tRegister 10 used at lines: ";
                sorter.clear();     // Sorter ready for reuse
                sorter.assign(register10Use.begin(), register10Use.end());
                std::sort(sorter.begin(), sorter.end());    // Sort register use
                for(auto& line : sorter)
                {
                    outfile << line << " ";
                }
                outfile << "\n\tRegister 11 used at lines: ";
                sorter.clear();     // Sorter ready for reuse
                sorter.assign(register11Use.begin(), register11Use.end());
                std::sort(sorter.begin(), sorter.end());    // Sort register use
                for(auto& line : sorter)
                {
                    outfile << line << " ";
                }
                outfile << "\n\tRegister 12 used at lines: ";
                sorter.clear();     // Sorter ready for reuse
                sorter.assign(register12Use.begin(), register12Use.end());
                std::sort(sorter.begin(), sorter.end());    // Sort register use
                for(auto& line : sorter)
                {
                    outfile << line << " ";
                }
                outfile << "\n\tRegister 13 used at lines: ";
                sorter.clear();     // Sorter ready for reuse
                sorter.assign(register13Use.begin(), register13Use.end());
                std::sort(sorter.begin(), sorter.end());    // Sort register use
                for(auto& line : sorter)
                {
                    outfile << line << " ";
                }
                outfile << "\n\tRegister 14 used at lines: ";
                sorter.clear();     // Sorter ready for reuse
                sorter.assign(register14Use.begin(), register14Use.end());
                std::sort(sorter.begin(), sorter.end());    // Sort register use
                for(auto& line : sorter)
                {
                    outfile << line << " ";
                }
                outfile << "\n\tRegister 15 used at lines: ";
                sorter.clear();     // Sorter ready for reuse
                sorter.assign(register15Use.begin(), register15Use.end());
                std::sort(sorter.begin(), sorter.end());    // Sort register use
                for(auto& line : sorter)
                {
                    outfile << line << " ";
                }
                outfile << "\n********************************************************\n";
                outfile << "SVC Use:\n";
                for(auto& line : svcUse)
                {
                    outfile << "\t" << line << "\n";
                }
                outfile << "Subroutine Use:\n";
                for(auto& line : subroutineUse)
                {
                    outfile << "\t" << line << "\n";
                }
                outfile << "Branch Use:\n";
                for(auto& line : branchUse)
                {
                    outfile << "\t" << line << "\n";
                }
                outfile << "Directive Use:\n";
                for (auto& map : directiveUse) 
                {
                    outfile << "\t" << map.first << " at lines: ";
                    for (size_t i = 0; i < map.second.size(); i++) 
                    {
                        outfile << map.second[i] << " ";
                    }
                    outfile << "\n";
                }
                outfile << "********************************************************\n";
                outfile << "Addressing Modes:\n";
                outfile << "\tLines with indirect addressing: ";
                for(auto& line : indirectMode)
                {
                    outfile << line << " ";
                }
                outfile << "\n\tLines with indirect addressing with offset: ";
                for(auto& line : indirectOffsetMode)
                {
                    outfile << line << " ";
                }
                outfile << "\n\tLines with auto, pre-index addressing: ";
                for(auto& line : preIndexMode)
                {
                    outfile << line << " ";
                }
                outfile << "\n\tLines with auto, post-index addressing: ";
                for(auto& line : postIndexMode)
                {
                    outfile << line << " ";
                }
                outfile << "\n\tLines with PC relative addressing: ";
                for(auto& line : pcRelativeMode)
                {
                    outfile << line << " ";
                }
                outfile << "\n\tLines with PC relative addressing with literal pool: ";
                for(auto& line : pcLiteralMode)
                {
                    outfile << line << " ";
                }
                outfile << "\n\tLines with uncertain addressing modes: ";
                for(auto& line : unsureMode)
                {
                    outfile << line << " ";
                }
                outfile << "\n********************************************************\n";

                outfile << "Errors found:\n";

                if (exitExists == false)
                {
                    outfile << "\tNo proper exit, svc 0, from program before .data section\n";
                }

                if(pushNum > popNum)
                {
                    outfile << "\tMore pushes detected than pops. Ensure that all values are popped off the heap.\n";
                }
                else if(pushNum < popNum)
                {
                    outfile << "\tMore pops detected than pushes. Ensure that there is always a value on the heap before a Pop.\n";
                }

                for(auto& line : stringError)
                {
                    outfile << "\t" << line << "\n";
                }
                for(auto& line : unwantedInstructions)
                {
                    outfile << "\t" << line << "\n";
                }
                for(auto& line : restrictedError)
                {
                    outfile << "\t" << line << "\n";
                }
                for(auto& line : unusedConditional)
                {
                    outfile << "\t" << line << "\n";
                }
                for(auto& line : unusedLabel)
                {
                    outfile << "\t" << line << "\n";
                }
                for(auto& line : unusedVariable)
                {
                    outfile << "\t" << line << "\n";
                }
                for(auto& line : unusedConstant)
                {
                    outfile << "\t" << line << "\n";
                }
                for(auto& line : isolatedCode)
                {
                    outfile << "\t" << line << "\n";
                }
                for(auto& line : noReturnError)
                {
                    outfile << "\t" << line << "\n";
                }
                for(auto& line : lrSaveError)
                {
                    outfile << "\t" << line << "\n";
                }
                for(auto& line : branchOutError)
                {
                    outfile << "\t" << line << "\n";
                }
                for(auto& line : registerError)
                {
                    outfile << "\t" << line << "\n";
                }
                outfile << "********************************************************\n";
                outfile.close();


                std::cout << "Created report file: " << output_file << "\n";
            }
            break;
    }

    infile.close(); // Make sure to always close file before exiting
} 
