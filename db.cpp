#include <iostream>
#include <string>
using namespace std;

/// Display constants
const string PROMPT = "> ";
    
/// @brief Console Input representation
struct InputBuffer {
    streamsize input_size = -1;
    string buffer;
};

/// @brief Represents the state of the meta command
enum MetaCommandState {
    META_COMMAND_SUCCESS,
    META_COMMAND_UNRECOGNIZED
};

/// @brief Represents the various SQL statements
enum StatementCommand {
    STATEMENT_SELECT,
    STATEMENT_INSERT,
    STATEMENT_DELETE
};

/// @brief Represents the state of prepare statement operation.
enum StatementPrepareState {
    PREPARE_SUCCESS,
    PREPARE_UNRECOGNIZED
};

/// @brief Prepare the display for taking the input.
void display_prompt() {
    cout << PROMPT;
}

void read_input(InputBuffer& input_buffer) {
    // check if input stream is ready for taking input, eg due to error flags
    // being set or EOF condition
    if (!cin.good()) {
        cout << "Input stream not ready for taking input, exiting..." << endl;
        exit(EXIT_FAILURE);
    }

    // read the input
    // Successfully captured the input
    if (getline(cin, input_buffer.buffer)) {
        input_buffer.input_size = input_buffer.buffer.size();
    }
    // Either an error of EOF case: Premature end of input using
    // Ctrl + D (Unix) or Ctrl + Z (Windows)
    else if (cin.eof()) {
        cout << "EOF reached, input stream closed prematurely, exiting..." << endl;
        input_buffer.input_size = 0;
        // clear the error state
        cin.clear();
        return;
    }
    else {
        cout << "Unexpected error while reading input, exiting..." << endl;
        input_buffer.input_size = -1;
        exit(EXIT_FAILURE);
    }
}

void repl_loop() {
    InputBuffer input_buffer;

    cout << "Starting REPL loop..." << endl;

    while (true) {
        display_prompt();
        // get the input
        read_input(input_buffer);
        
        cout << "Input: " << input_buffer.buffer << "Size: " << input_buffer.input_size << endl;

        if (input_buffer.buffer == "exit") {
            cout << "Exiting..." << endl;
            exit(EXIT_SUCCESS);
        }
        else {
            cout << "Unrecognized command: " << input_buffer.buffer << endl;
        }
    }
}

int main(int argc, char** argv) {
    repl_loop();
    return 0;
}