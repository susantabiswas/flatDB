#include <cstdint>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
using namespace std;

#define size_of_attribute(Struct, Attribute) sizeof(((Struct*)0)->Attribute)

/// Display constants
const string PROMPT = "> ";
bool DEBUG_MODE = false;

/*
* Column size constants
*/
const uint16_t USERNAME_LENGTH = 32;
const uint16_t EMAIL_LENGTH = 255;

/*
*   Table and Page size
*/
const uint16_t PAGE_SIZE = 4 * 1024; // 4KB
const uint16_t TABLE_MAX_PAGES = 100;

/// @brief Represents the state of the meta command
enum MetaCommandResult {
    META_COMMAND_SUCCESS,
    META_COMMAND_UNRECOGNIZED
};

enum ExecuteResult {
    EXECUTE_SUCCESS,
    EXECUTE_TABLE_FULL,
    EXECUTE_FAILURE
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
    PREPARE_INVALID_SYNTAX,
    PREPARE_TOKEN_TOO_LONG,
    PREPARE_NULL_TOKEN,
    PREPARE_TOKEN_NEGATIVE,
    PREPARE_UNRECOGNIZED
};

/// @brief Represents the outcome of reading input from console.
enum InputResult {
    SUCCESS,
    EOF_REACHED,
    STREAM_ERROR,
    INVALID_INPUT
};

/// @brief Console Input representation
struct InputBuffer {
    streamsize input_size = -1;
    string buffer;
};

struct Row {
    long long id;
    char username[USERNAME_LENGTH + 1]; // +1 for null terminator
    char email[EMAIL_LENGTH + 1]; // +1 for null terminator
};

struct Statement {
    StatementCommand statement_command;
    Row row;
};

struct Table {
    uint32_t num_rows;
    // Free list of pages
    // Each entry points to a memory page which
    // is nothing but a block of memory and multiple rows
    // can be stored in it.
    void* pages[TABLE_MAX_PAGES]; 
};

/*
*   Row layout related
*/
const uint32_t ID_SIZE = size_of_attribute(Row, id);
const uint32_t ID_OFFSET = 0;
const uint32_t USERNAME_SIZE = size_of_attribute(Row, username);
const uint32_t USERNAME_OFFSET = ID_OFFSET + ID_SIZE;
const uint32_t EMAIL_SIZE = size_of_attribute(Row, email);
const uint32_t EMAIL_OFFSET = USERNAME_OFFSET + USERNAME_SIZE;
const uint32_t ROW_SIZE = ID_SIZE + USERNAME_SIZE + EMAIL_SIZE;

/*
* Storage related constants
*/
const uint32_t ROWS_PER_PAGE = PAGE_SIZE / ROW_SIZE;
const uint32_t TABLE_MAX_ROWS = TABLE_MAX_PAGES * ROWS_PER_PAGE;

/*
*   Factory methods
*/
Table create_table_factory() {
    Table table;
    table.num_rows = 0;

    for(uint32_t i = 0; i < TABLE_MAX_PAGES; i++)
        table.pages[i] = nullptr;
    return table;
}

void free_table(Table& table) {
    for(uint32_t i = 0; i < TABLE_MAX_PAGES; i++) {
        if (table.pages[i] != nullptr) {
            free(table.pages[i]);
            table.pages[i] = nullptr;
        }
    }
}

/*
*   Row and Table related operations
*/
void write_row(void* row_slot, Row& row) {
    memcpy((char*)row_slot + ID_OFFSET, &(row.id), ID_SIZE);
    memcpy((char*)row_slot + USERNAME_OFFSET, &(row.username), USERNAME_SIZE);
    memcpy((char*)row_slot + EMAIL_OFFSET, &(row.email), EMAIL_SIZE);
}

void read_row(void* row_slot, Row& row) {
    memcpy(&(row.id), (char*)row_slot + ID_OFFSET, ID_SIZE);
    memcpy(&(row.username), (char*)row_slot + USERNAME_OFFSET, USERNAME_SIZE);
    memcpy(&(row.email), (char*)row_slot + EMAIL_OFFSET, EMAIL_SIZE);
}

void print_row(Row& row) {
    cout << "[Row] ID: " << row.id << ", Username: " << row.username << ", Email: " << row.email << endl;
}

/// @brief Prepare the display for taking the input.
void display_prompt() {
    cout << PROMPT;
}

void init_db_info() {
    if (DEBUG_MODE) {
        cout << "TABLE_MAX_ROWS: " << TABLE_MAX_ROWS << ", ROW_SIZE: " << ROW_SIZE << endl;
        cout << "TABLE_MAX_PAGES: " << TABLE_MAX_PAGES << ", PAGE_SIZE: " << PAGE_SIZE << ", ROWS_PER_PAGE: " << ROWS_PER_PAGE << endl;
    }
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

vector<string> tokenize_string(string& str, char delimiter = ' ') {
    istringstream ss(str);
    string token;

    vector<string> tokens;

    while (std::getline(ss, token, delimiter)) {
        tokens.push_back(token);
    }

    return tokens;
}

pair<StatementPrepareState, Statement> prepare_insert(string& cmd) {
    Statement statement;

    vector<string> tokens = tokenize_string(cmd, ' ');

    // Syntax: insert id username email
    if (tokens.size() < 4) {
        return { PREPARE_INVALID_SYNTAX, statement };
    }

    for(string token: tokens) {
        if (token.empty()) {
            return { PREPARE_NULL_TOKEN, statement };
        }
    }

    if (tokens[2].size() > USERNAME_LENGTH || tokens[3].size() > EMAIL_LENGTH) {
        return { PREPARE_TOKEN_TOO_LONG, statement };
    }
    
    statement.row.id = stoll(tokens[1]);

    if (statement.row.id < 0) {
        return { PREPARE_TOKEN_NEGATIVE, statement };
    }

    strncpy(statement.row.username, tokens[2].c_str(), USERNAME_LENGTH);
    strncpy(statement.row.email, tokens[3].c_str(), EMAIL_LENGTH);

    // explictly null terminate the str to avoid any issues
    statement.row.username[USERNAME_LENGTH] = '\0';
    statement.row.email[EMAIL_LENGTH] = '\0';

    if (DEBUG_MODE)
        print_row(statement.row);
    statement.statement_command = STATEMENT_INSERT;
    return { PREPARE_SUCCESS, statement };
}

pair<StatementPrepareState, Statement> prepare_statement_command(string& cmd) {
    Statement statement;

    // parse the statement
    if (cmd.substr(0, 6) == "insert") {
        return prepare_insert(cmd);
    }
    else if (cmd == "select") {
        // Syntax: select
        statement.statement_command = STATEMENT_SELECT;
        return { PREPARE_SUCCESS, statement };
    }
    else if (cmd == "delete")
        return { PREPARE_SUCCESS, statement };
    else
        return { PREPARE_UNRECOGNIZED, statement };
}

void* get_row_slot(int32_t row_num, Table& table) {
    // NOTE: For now, we take the row index as the
    // next row after the last inserted row
    int32_t page_idx = row_num / ROWS_PER_PAGE;

    void* page = table.pages[page_idx];

    if (page == nullptr) {
        page = malloc(PAGE_SIZE);
        // since the page is allocated memory, set the address
        table.pages[page_idx] = page;
    }

    uint32_t row_offset = row_num % ROWS_PER_PAGE;
    uint32_t byte_offset = row_offset * ROW_SIZE;

    // NOTE: Ptr arithmetic doesnt work on void*, since char* is 1 byte, we cast it to char*
    // and it is implictly casted to void* when returned
    char* row_addr = static_cast<char*>(page) + byte_offset;

    if (DEBUG_MODE)
        cout << "Table: " << &table << ", RowAddrs: " << static_cast<void*>(row_addr) << " , Row_num: " << row_num << ", Page_idx: " << page_idx << ", Row_offset: " << row_offset << ", Byte_offset: " << byte_offset << endl;
    return row_addr;
}

ExecuteResult execute_insert(Statement& statement, Table& table) {
    if (table.num_rows >= TABLE_MAX_ROWS) {
        return EXECUTE_TABLE_FULL;
    }

    // find a free slot in the table
    void* row_slot = get_row_slot(table.num_rows, table);
    write_row(row_slot, statement.row);
    ++table.num_rows;

    Row row;
    read_row(row_slot, row);

    if (DEBUG_MODE)
        cout <<"[INSERT] Id: " << row.id << " " << row.username << " " << row.email << endl;
    
    cout << "Row inserted successfully." << endl;
    return EXECUTE_SUCCESS;
}

ExecuteResult execute_select_all(Table& table) {
    Row row;

    for(uint32_t i = 0; i < table.num_rows; i++) {
        read_row(get_row_slot(i, table), row);
        cout <<"[SELECT] (" << row.id << " " << row.username << " " << row.email << ")" << endl;
    }

    cout << "Returned " << table.num_rows << " rows." << endl;
    return EXECUTE_SUCCESS;
}

ExecuteResult execute_statement(Statement statement, Table& table) {
    
    switch (statement.statement_command) {
        case STATEMENT_INSERT:
            return execute_insert(statement, table);
        case STATEMENT_SELECT:
            return execute_select_all(table);
        case STATEMENT_DELETE:
            return EXECUTE_SUCCESS;
    }

    return EXECUTE_FAILURE;
}

void repl_loop() {
    InputBuffer input_buffer;
    Table table = create_table_factory();
    init_db_info();

    while (true) {
        display_prompt();

        // get the input
        InputResult input_res = read_input(input_buffer);
        
        // Handle input result cases
        if (input_res != InputResult::SUCCESS) {
            cerr << "Error reading input, exiting." << endl;
            exit(EXIT_FAILURE);
        }

        if (input_buffer.buffer.size() == 0) {
            cout << "Empty input, please try again." << endl;
            continue;
        }

        if (DEBUG_MODE)
            cout << "Input: " << input_buffer.buffer << ", size: " << input_buffer.input_size << endl;
        
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
            case PREPARE_INVALID_SYNTAX:
                cout << "Invalid Syntax: " << input_buffer.buffer << endl;
                continue;
            case PREPARE_TOKEN_TOO_LONG:
                cout << "Token too long: " << input_buffer.buffer << endl;
                continue;
            case PREPARE_NULL_TOKEN:
                cout << "Null token found: " << input_buffer.buffer << endl;
                continue;
            case PREPARE_TOKEN_NEGATIVE:
                cout << "Negative token found: " << input_buffer.buffer << endl;
                continue;
            case PREPARE_UNRECOGNIZED:
                cout << "Unrecognized statement: " << input_buffer.buffer << endl;
                continue;
        }

        // Once the statement preparation is completed, execute it
        switch(execute_statement(statement, table)) {
            case EXECUTE_SUCCESS:
                break;
            case EXECUTE_TABLE_FULL:
                cout << "[ERROR] Table is full, cannot insert the row" << endl;
                break;
        }
    }
  
    free_table(table);
}

void parse_main_args(int argc, char** argv) {
    for(int i = 0; i < argc; i++) {
        string arg = argv[i];
        cout << arg << endl;
        if(arg == "--debug" || arg == "-d") {
            DEBUG_MODE = true;
            cout << "Debug mode enabled." << endl;
        }
    }
}

int main(int argc, char** argv) {
    parse_main_args(argc, argv);
    repl_loop();
    
    return 0;
}