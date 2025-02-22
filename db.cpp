#include <cstdint>
#include <iostream>
#include <string>
using namespace std;

#define size_of_attribute(Struct, Attribute) sizeof(((Struct*)0)->Attribute)

/// Display constants
const string PROMPT = "> ";

/*
* Column size constants
*/
const uint16_t USERNAME_LENGTH = 32;
const uint16_t EMAIL_LENGTH = 255;

/*
*   Table and Page size
*/
const uint16_t PAGE_SIZE = 4 * 1024; // 4KB
const uint16_t TABLE_MAX_PAGES = 1000;

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
    char username[USERNAME_LENGTH];
    char email[EMAIL_LENGTH];
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

/*
*   Row and Table related operations
*/
void write_row(void* row_slot, Row& row) {
    memcpy(row_slot + ID_OFFSET, &(row.id), ID_SIZE);
    memcpy(row_slot + USERNAME_OFFSET, &(row.username), USERNAME_SIZE);
    memcpy(row_slot + EMAIL_OFFSET, &(row.email), EMAIL_SIZE);
}

void read_row(void* row_slot, Row& row) {
    memcpy(&(row.id), row_slot + ID_OFFSET, ID_SIZE);
    memcpy(&(row.username), row_slot + USERNAME_OFFSET, USERNAME_SIZE);
    memcpy(&(row.email), row_slot + EMAIL_OFFSET, EMAIL_SIZE);
}

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

pair<StatementPrepareState, Statement> prepare_statement_command(string& cmd) {
    Statement statement;

    // parse the statement
    if (cmd.substr(0, 6) == "insert") {
        // Syntax: insert id username email
        int assigned = sscanf(
            cmd.c_str(), 
            "insert %lld %s %s", &statement.row.id, &statement.row.username, &statement.row.email);

        if (assigned < 3) {
            return { PREPARE_INVALID_SYNTAX, statement };
        }
        statement.statement_command = STATEMENT_INSERT;
        return { PREPARE_SUCCESS, statement };
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

    cout << "Table: " << &table << ", RowAddrs: " << page + byte_offset << " , Row_num: " << row_num << ", Page_idx: " << page_idx << ", Row_offset: " << row_offset << ", Byte_offset: " << byte_offset << endl;
    return page + byte_offset;
}

ExecuteResult execute_insert(Statement& statement, Table& table) {
    if (table.num_rows >= TABLE_MAX_ROWS) {
        cout << "[ERROR] Table is full" << endl;
        return EXECUTE_TABLE_FULL;
    }

    // find a free slot in the table
    void* row_slot = get_row_slot(table.num_rows, table);
    write_row(row_slot, statement.row);
    ++table.num_rows;

    Row row;
    read_row(row_slot, row);

    cout <<"[INSERT] Row_idx: " << row_slot << ", Id: " << row.id << " " << row.username << " " << row.email << endl;
    
    return EXECUTE_SUCCESS;
}

ExecuteResult execute_select_all(Table& table) {
    Row row;

    for(uint32_t i = 0; i < table.num_rows; i++) {
        read_row(get_row_slot(i, table), row);
        cout <<"[READ] Row_idx: " << get_row_slot(i, table) << ", Id: " << row.id << " " << row.username << " " << row.email << endl;
    }

    return EXECUTE_SUCCESS;
}

ExecuteResult execute_statement(Statement statement, Table& table) {
    cout << "Statement " << statement.statement_command << " executed successfully..." << endl;

    switch (statement.statement_command) {
        case STATEMENT_INSERT:
            return execute_insert(statement, table);
        case STATEMENT_SELECT:
            return execute_select_all(table);
        case STATEMENT_DELETE:
            return EXECUTE_SUCCESS;
    }
}

void repl_loop() {
    InputBuffer input_buffer;
    Table table = create_table_factory();

    cout << "Starting REPL loop..." << endl;

    while (true) {
        display_prompt();

        // get the input
        InputResult input_res = read_input(input_buffer);
        
        // Handle input result cases
        if (input_res != InputResult::SUCCESS) {
            cerr << "Error reading input, exiting..." << endl;
            exit(EXIT_FAILURE);
        }

        if (input_buffer.buffer.size() == 0) {
            cout << "Empty input, please try again..." << endl;
            continue;
        }

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
            case PREPARE_UNRECOGNIZED:
                cout << "Unrecognized statement: " << input_buffer.buffer << endl;
                continue;
        }

        // Once the statement preparation is completed, execute it
        switch(execute_statement(statement, table)) {
            case EXECUTE_SUCCESS:
                cout << "Statement executed successfully..." << endl;
                break;
            case EXECUTE_TABLE_FULL:
                cout << "Table is full, cannot insert the row..." << endl;
                break;
        }
    }
}

int main(int argc, char** argv) {
    repl_loop();
    return 0;
}