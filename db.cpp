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
enum MetaCommandResult {
    META_COMMAND_SUCCESS,
    META_COMMAND_UNRECOGNIZED
};

/// @brief Represents the various SQL statements
enum StatementCommand {
    STATEMENT_SELECT,
    STATEMENT_INSERT,
    STATEMENT_DELETE,
    STATEMENT_UNRECOGNIZED
};

/// @brief Represents the state of prepare statement operation.
enum StatementPrepareState {
    PREPARE_SUCCESS,
    PREPARE_UNRECOGNIZED
};

/// @brief Represents the outcome of reading input from console.
enum InputResult {
    SUCCESS,
    EOF_REACHED,
    STREAM_ERROR,
    INVALID_INPUT
};

/// @brief Prepare the display for taking the input.
void display_prompt() {
    cout << PROMPT;
}

InputResult read_input(InputBuffer& input_buffer) {
    // check if input stream is ready for taking input, eg due to error flags
    // being set or EOF condition
    if (!cin.good()) {
        cerr << "Input stream not ready for taking input, exiting..." << endl;
        return InputResult::STREAM_ERROR;
    }

    // read the input
    // Successfully captured the input
    if (getline(cin, input_buffer.buffer)) {
        input_buffer.input_size = input_buffer.buffer.size();
        return InputResult::SUCCESS;
    }

    // Either an error of EOF case: Premature end of input using
    // Ctrl + D (Unix) or Ctrl + Z (Windows)
    if (cin.eof()) {
        cout << "EOF reached, input stream closed prematurely, exiting..." << endl;
        input_buffer.input_size = 0;
        // clear the error state
        cin.clear();
        return InputResult::EOF_REACHED;
    }

    cout << "Unexpected error while reading input, exiting..." << endl;
    input_buffer.input_size = -1;
    return InputResult::INVALID_INPUT;
}

MetaCommandResult run_metacommand(string& cmd) {
    if (cmd == ".exit") {
        cout << "Encountered exit, exiting..." << endl;
        exit(EXIT_SUCCESS);
    }
    else {
        return MetaCommandResult::META_COMMAND_UNRECOGNIZED;
    }
}


pair<StatementPrepareState, StatementCommand> prepare_statement_command(string& cmd) {
    if (cmd == "insert")
        return { PREPARE_SUCCESS, StatementCommand::STATEMENT_INSERT };
    else if (cmd == "select")
        return { PREPARE_SUCCESS, StatementCommand::STATEMENT_SELECT };
    else if (cmd == "delete")
        return { PREPARE_SUCCESS, StatementCommand::STATEMENT_DELETE };
    else
        return { PREPARE_UNRECOGNIZED, StatementCommand::STATEMENT_UNRECOGNIZED };
}

bool execute_statement(StatementCommand statement) {
    cout << "Statement " << statement << " executed successfully..." << endl;
    return true;
}

void repl_loop() {
    InputBuffer input_buffer;

    cout << "Starting REPL loop..." << endl;

    while (true) {
        display_prompt();

        // get the input
        InputResult input_res = read_input(input_buffer);
        
        if (input_res != InputResult::SUCCESS) {
            cerr << "Error reading input, exiting..." << endl;
            exit(EXIT_FAILURE);
        }

        if (input_buffer.buffer.size() == 0) {
            cout << "Empty input, please try again..." << endl;
            continue;
        }

        cout << "Input: " << input_buffer.buffer << "Size: " << input_buffer.input_size << endl;
        
        // Handle meta commands, meta commands start with a '.' character
        if (input_buffer.buffer[0] == '.') {
            switch (run_metacommand(input_buffer.buffer)) {
                case MetaCommandResult::META_COMMAND_SUCCESS:
                    continue;
                case MetaCommandResult::META_COMMAND_UNRECOGNIZED:
                    cout << "Unrecognized command: " << input_buffer.buffer << endl;
                    continue;
            }
        }
        
        // Prepare the statement commands which can then be executed
        // Here the idea is to convert the string to a more code friendly semantic
        // Eg converting "insert" to StatementCommand::STATEMENT_INSERT
        auto [prepare_state, statement] = prepare_statement_command(input_buffer.buffer);

        switch (prepare_state) {
            case PREPARE_SUCCESS:
                break;
            case PREPARE_UNRECOGNIZED:
                cout << "Unrecognized statement: " << input_buffer.buffer << endl;
                continue;
        }

        // Once the statement preparation is completed, execute it
        execute_statement(statement);
    }
}

int main(int argc, char** argv) {
    repl_loop();
    return 0;
}