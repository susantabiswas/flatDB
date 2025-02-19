#include <iostream>
using namespace std;

/// Display constants
const string PROMPT = "> ";
    

struct InputBuffer {
    streamsize input_size = -1;
    string buffer;
};

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
    string input;

    InputBuffer input_buffer;

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