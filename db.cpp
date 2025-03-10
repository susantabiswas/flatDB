#include <cstdint>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
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

/// @brief Represents the various database statements
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

enum NodeType {
    INTERNAL,
    LEAF
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

struct Pager {
    int file_descriptor;
    uint32_t file_length;
    uint32_t num_pages;
    
    // cache of pages in memory
    void* pages[TABLE_MAX_PAGES];
};

struct Table {
    Pager pager;
    uint32_t num_rows;
    uint32_t root_page_num;
};

struct Cursor {
    Table* table;
    uint32_t page_num; // 0 indexed
    uint32_t cell_num; // 0 indexed
    bool end_of_table; // whether the cursor is at the end of table.
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
 * @brief B+ Tree Node Metadata 
 */
///////////// Common Node Header Layout //////////////
// Unlike the plain array storage implementation, when we use a tree, we 
// will need extra metadata to manage the tree structure and the context for
// each node. Each node will have a common header layout which will be used.

// NODE_TYPE(1 byte) | IS_ROOT(1 byte) | PARENT_POINTER(4 bytes)
const uint32_t NODE_TYPE_SIZE = sizeof(NodeType);
const uint32_t NODE_TYPE_OFFSET = 0;
const uint32_t IS_ROOT_SIZE = sizeof(bool);
const uint32_t IS_ROOT_OFFSET = NODE_TYPE_SIZE;
const uint32_t PARENT_POINTER_SIZE = sizeof(uint32_t);
const uint32_t PARENT_POINTER_OFFSET = IS_ROOT_OFFSET + IS_ROOT_SIZE;
const uint32_t COMMON_NODE_HEADER_SIZE =
    NODE_TYPE_SIZE + IS_ROOT_SIZE + PARENT_POINTER_SIZE;

//////////// Leaf Node Header Layout //////////////
// A leaf node will also need to track how many cells are part of it.
// An internal node in B+ tree doesnt store data so this is only required
// for the leaf nodes.

// COMMON_HEADER + NUM_CELLS(4 bytes)
const uint32_t LEAF_NODE_NUM_CELLS = sizeof(uint32_t);
const uint32_t LEAF_NODE_NUM_CELLS_OFFSET = COMMON_NODE_HEADER_SIZE;
const uint32_t LEAF_NODE_HEADER_SIZE = COMMON_NODE_HEADER_SIZE + LEAF_NODE_NUM_CELLS;

//////////// Leaf Node Body Layout //////////////
const uint32_t LEAF_NODE_KEY_SIZE = sizeof(uint32_t);
const uint32_t LEAF_NODE_KEY_OFFSET = 0;
const uint32_t LEAF_NODE_VALUE_SIZE = ROW_SIZE;
const uint32_t LEAF_NODE_VALUE_OFFSET = 
    LEAF_NODE_KEY_OFFSET + LEAF_NODE_KEY_SIZE;
const uint32_t LEAF_NODE_CELL_SIZE = 
    LEAF_NODE_KEY_SIZE + LEAF_NODE_VALUE_SIZE;
const uint32_t LEAF_NODE_SPACE_FOR_CELLS = 
    PAGE_SIZE - LEAF_NODE_HEADER_SIZE;
const uint32_t LEAF_NODE_MAX_CELLS =
    LEAF_NODE_SPACE_FOR_CELLS / LEAF_NODE_CELL_SIZE;


/*
* Leaf node accessors
*/
uint32_t* get_leaf_node_num_cells_offset(void* node) {
    return static_cast<uint32_t*>(node) + LEAF_NODE_NUM_CELLS_OFFSET;
}

void init_leaf_node(void* node) {
    uint32_t* num_cells = get_leaf_node_num_cells_offset(node);
    *num_cells = 0;
}

// Get the address where the no. of cells for a node is stored
uint32_t* get_leaf_node_cells(void* node) {
    return static_cast<uint32_t*>(node) + LEAF_NODE_NUM_CELLS_OFFSET;
}

void* get_leaf_node_cell(void* node, uint32_t cell_idx) {
    // Node: Header + Cell_0 + Cell_1 + ... + Cell_n
    // Cell_i = Key + Value (Row)
    return static_cast<char*>(node) + LEAF_NODE_HEADER_SIZE + (cell_idx * LEAF_NODE_CELL_SIZE);
}

uint32_t* get_leaf_node_key(void* node, uint32_t cell_idx) {
    return static_cast<uint32_t*>(get_leaf_node_cell(node, cell_idx));
}

void* get_leaf_node_value(void* node, uint32_t cell_idx) {
    void* cell = get_leaf_node_cell(node, cell_idx);
    return static_cast<char*>(cell) + LEAF_NODE_VALUE_OFFSET;
}


/*
 *   Factory methods
 */
Pager pager_factory(int fd, uint32_t file_length) {
    Pager pager;

    for(uint32_t i = 0; i < TABLE_MAX_PAGES; i++)
        pager.pages[i] = nullptr;

    pager.file_descriptor = fd;
    pager.file_length = file_length;
    pager.num_pages = file_length / PAGE_SIZE;
    
    return pager;
}

void free_table(Table& table) {
    for(uint32_t i = 0; i < TABLE_MAX_PAGES; i++) {
        if (table.pager.pages[i] != nullptr) {
            free(table.pager.pages[i]);
            table.pager.pages[i] = nullptr;
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

void* get_page(Pager& pager, uint32_t page_idx) {
    if(page_idx >= TABLE_MAX_PAGES) {
        cerr << "Page index out of bounds: " << page_idx << endl;
        exit(EXIT_FAILURE);
    }

    // cache miss
    if (pager.pages[page_idx] == nullptr) {
        void* page = malloc(PAGE_SIZE);
        memset(page, 0, PAGE_SIZE);

        if (page == nullptr) {
            cerr << "Unable to allocate memory for page" << endl;
            exit(EXIT_FAILURE);
        }

        uint32_t num_pages = pager.file_length / PAGE_SIZE;

        // Case: There can be scenario where the write op might have been
        // disrupted (eg shutdown etc) and the last page was not written completely
        // and only a part of the entire page was written. To handle that we treat
        // that as complete page and let the system read the data till the pt which is 
        // avail.
        if(pager.file_length % PAGE_SIZE) {
            cout << "[WRN] Partial page found at the end of file" << endl;
            num_pages += 1;
        }

        // if the request page is within the existing pages
        if (page_idx < num_pages) {
            // go to the starting position of this page and then load the page
            lseek(pager.file_descriptor, page_idx * PAGE_SIZE, SEEK_SET);
            ssize_t bytes_read = read(pager.file_descriptor, page, PAGE_SIZE);

            if (bytes_read == -1) {
                cerr << "Error reading file: " << errno << endl;
                exit(EXIT_FAILURE);
            }
        }

        // cache the page
        pager.pages[page_idx] = page;    
        
        // if this page didnt existed before, update the num_pages
        if(page_idx >= num_pages) {
            pager.num_pages = page_idx + 1;
            
            if (DEBUG_MODE)
                cout << "Page Added: Idx: " << page_idx << ", Num_pages: " << pager.num_pages << endl;
        }
    }
    
    return pager.pages[page_idx];
}

Cursor table_begin(Table& table) {
    Cursor cursor;
    cursor.table = &table;
    cursor.page_num = table.root_page_num;
    cursor.cell_num = 0;

    // get the page of root node and using that check if there
    // are cells in the tree
    void* page = get_page(table.pager, table.root_page_num);
    uint32_t num_cells = *get_leaf_node_cells(page);
    // if there are no leaf nodes, then the table is empty
    cursor.end_of_table = (num_cells == 0);

    return cursor;
}

Cursor table_end(Table& table) {
    Cursor cursor;
    cursor.table = &table;
    cursor.page_num = table.root_page_num;

    void* page = get_page(table.pager, table.root_page_num);
    uint32_t num_cells = *get_leaf_node_cells(page);
    cursor.cell_num = num_cells;

    cursor.end_of_table = true;

    return cursor;
}

void* get_cursor_value_addr(Cursor& cursor) {
    // NOTE: For now, we take the row index (0 indexed) as the
    // next row after the last inserted row
    // page_idx is again 0 indexed
    int32_t page_idx = cursor.page_num;

    void* page = get_page(cursor.table->pager, page_idx);

    void* cell_val_addr = get_leaf_node_value(page, cursor.cell_num);

    if (DEBUG_MODE)
        cout << "CellAddrs: " << static_cast<void*>(cell_val_addr) << " , cell_num: " << cursor.cell_num << ", Page_idx: " << page_idx << endl;
    return cell_val_addr;
}

void cursor_next(Cursor& cursor) {
    ++cursor.cell_num;
    uint32_t page_num = cursor.page_num;

    void* page = get_page(cursor.table->pager, page_num);
    uint32_t num_cells = *get_leaf_node_cells(page);

    if (cursor.cell_num >= num_cells)
        cursor.end_of_table = true;
}

Pager open_pager(string filename) {
    int fd = open(
        filename.c_str(),
        O_RDWR | // R/W mode 
            O_CREAT, // Create file if it does not exist
        S_IWUSR | // User Write permission
            S_IRUSR // User Read permission
    );
    
    if (fd == -1) {
        cout << "Unable to open file: " << filename << endl;
        exit(EXIT_FAILURE);
    }

    // Position the fd to the last pos to get the file len
    off_t file_len = lseek(fd, 0, SEEK_END);
    // reposition to beginning of file
    lseek(fd, 0, SEEK_SET);
    
    Pager pager = pager_factory(fd, file_len);

    return pager;
}

Table open_db_conn(string filename) {
    Table table;
    
    table.pager = open_pager(filename);
    table.root_page_num = 0;

    // New database, so initialize the first page as leaf node
    if (table.pager.num_pages == 0) {
        void* root = get_page(table.pager, 0);
        init_leaf_node(root);
    }

    int32_t num_rows = table.pager.file_length / ROW_SIZE;
    
    // Case: Partial last page -> In the last page, there might be only few rows written and the
    // remaining row slots are empty, detect the actual rows in the last page.
    // For a partially filled page, we still load the entire page in the memory and hence we
    // need to know how many rows are actually filled in the last page.
    if (num_rows > 0) {
        Row empty_row = {0, "", ""};
        Row row; 

        void* last_page = get_page(table.pager, table.pager.num_pages - 1);

        int32_t empty_rows = 0;
        for(int i = 0; i < ROWS_PER_PAGE; i++) {
            read_row(last_page + i * ROW_SIZE, row);
            
            if (memcmp(&row, &empty_row, ROW_SIZE) == 0) {
                empty_rows++;
            }
        }

        num_rows -= empty_rows;

        if (DEBUG_MODE)
            cout << "Found " << empty_rows << " empty rows in the last page" << endl;
    }

    table.num_rows = num_rows;
    
    if (DEBUG_MODE)
        cout << "Loaded " << num_rows << " rows." << endl;

    return table;
}

void flush_page(Pager& pager, uint32_t page_idx) {
    int fd = pager.file_descriptor;

    if (page_idx >= pager.num_pages) {
        cerr << "Page index is out of bounds: " << page_idx << endl;
        exit(EXIT_FAILURE);
    }

    if (pager.pages[page_idx] == nullptr) {
        cerr << "Null page cannot be flushed" << endl;
        exit(EXIT_FAILURE);
    }
    
    ssize_t pos = lseek(fd, page_idx * PAGE_SIZE, SEEK_SET);
    if(pos == -1) {
        cerr << "Error seeking the file: " << errno << endl;
        exit(EXIT_FAILURE);
    }

    ssize_t bytes_written = write(fd, pager.pages[page_idx], PAGE_SIZE);

    if (bytes_written == -1) {
        cerr << "Failed to save the data to disk." << endl;
        exit(EXIT_FAILURE);
    }
}

void close_db_conn(Table& table) {
    Pager pager = table.pager;

    // flush the database to disk
    for(uint32_t i = 0; i < pager.num_pages; i++) {
        if(pager.pages[i]) {
            flush_page(pager, i);
            free(pager.pages[i]);
            pager.pages[i] = nullptr;
        }
    }

    // close the fd and free up the pages
    int result = close(pager.file_descriptor);
    if(result == -1) {
        cerr << "Error closing file descriptor: " << errno << endl;
        exit(EXIT_FAILURE);
    }

}

void* insert_leaf_node(Cursor cursor, uint32_t key, Row row) {
    void* node = get_page(cursor.table->pager, cursor.page_num);
    uint32_t num_cells = *get_leaf_node_num_cells_offset(node);

    // Case: Leaf node is full
    if(num_cells >= LEAF_NODE_MAX_CELLS) {
        cerr << "Leaf node is full, cannot insert more cells" << endl;
        exit(EXIT_FAILURE);
    }

    // the cursor points to the position where the row should be inserted
    // To insert that row at ith pos, we move all the i ... nth cells to i+1 ... n+1
    if (cursor.cell_num < num_cells) {
        for(uint32_t i = num_cells; i > cursor.cell_num; i--)
            memcpy(get_leaf_node_cell(node, i), get_leaf_node_cell(node, i-1), ROW_SIZE);
    }

    // insert the row at the ith position
    *(get_leaf_node_key(node, cursor.cell_num)) = key;
    void* row_addr = get_leaf_node_value(node, cursor.cell_num);
    write_row(row_addr, row);

    // cell count is increased
    *(get_leaf_node_cells(node)) += 1;

    return static_cast<char*>(row_addr);
}

/// @brief Prepare the display for taking the input.
void display_prompt() {
    cout << PROMPT;
}

void display_leaf_node(void* node) {
    // displays the row cells
    uint32_t num_cells = *get_leaf_node_cells(node);
    
    cout << "Page addr: " << static_cast<char*>(node) << ", Total cells: " << num_cells << endl;
    for(uint32_t i = 0; i < num_cells; i++) {
        cout << "cell " << i << ": " << " key: " << *get_leaf_node_key(node, i) << " ";
        
        if (DEBUG_MODE) {
            Row row;
            read_row(get_leaf_node_value(node, i), row);
            print_row(row);
        }
        cout << endl;
    }
}

void init_db_info() {
    if (DEBUG_MODE) {
        cout << "TABLE_MAX_ROWS: " << TABLE_MAX_ROWS << ", ROW_SIZE: " << ROW_SIZE << endl;
        cout << "TABLE_MAX_PAGES: " << TABLE_MAX_PAGES << ", PAGE_SIZE: " << PAGE_SIZE << ", ROWS_PER_PAGE: " << ROWS_PER_PAGE << endl;
    
        cout << "BTree info..." << endl;
        cout << "............Common Header............" << endl;
        cout << "NODE_TYPE_SIZE: " << NODE_TYPE_SIZE << ", IS_ROOT_SIZE: " << IS_ROOT_SIZE << ", PARENT_POINTER_SIZE: " << PARENT_POINTER_SIZE << endl;
        cout << "COMMON_NODE_HEADER_SIZE: " << COMMON_NODE_HEADER_SIZE << endl;
        cout << "............Leaf Node Header............" << endl;
        cout << "LEAF_NODE_NUM_CELLS: " << LEAF_NODE_NUM_CELLS << ", LEAF_NODE_NUM_CELLS_OFFSET: " << LEAF_NODE_NUM_CELLS_OFFSET << endl;
        cout << "LEAF_NODE_HEADER_SIZE: " << LEAF_NODE_HEADER_SIZE << endl;
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

MetaCommandResult run_metacommand(string& cmd, Table& table) {
    if (cmd == ".exit") {
        cout << "Encountered exit, exiting..." << endl;
        close_db_conn(table);
        exit(EXIT_SUCCESS);
    }
    else if(cmd == ".btree") {
        cout << "Printing B+ Tree..." << endl;
        display_leaf_node(get_page(table.pager, table.root_page_num));
        return MetaCommandResult::META_COMMAND_SUCCESS;
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
    // NOTE: For now, we take the row index (0 indexed) as the
    // next row after the last inserted row
    // page_idx is again 0 indexed
    int32_t page_idx = row_num / ROWS_PER_PAGE;

    void* page = get_page(table.pager, page_idx);

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
    void* node = get_page(table.pager, table.root_page_num);

    if (*get_leaf_node_cells(node) >= LEAF_NODE_MAX_CELLS) {
        return EXECUTE_TABLE_FULL;
    }

    // row is inserted at the end of table
    Cursor cursor = table_end(table);

    Row row = statement.row;
    void* row_addr = insert_leaf_node(cursor, row.id, row);
    ++table.num_rows;

    if (DEBUG_MODE) {
        Row row;
        read_row(row_addr, row);
        cout <<"[INSERT] Id: " << row.id << " " << row.username << " " << row.email << endl;
    }

    cout << "Row inserted successfully." << endl;
    return EXECUTE_SUCCESS;
}

ExecuteResult execute_select_all(Table& table) {
    Row row;

    // Get the cursor to the beginning of table
    Cursor cursor = table_begin(table);
    while(!cursor.end_of_table) {
        void* cursor_addr = get_cursor_value_addr(cursor);
        read_row(cursor_addr, row);
        cursor_next(cursor);

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

void repl_loop(string filename) {
    InputBuffer input_buffer;
    Table table = open_db_conn(filename);
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
            switch (run_metacommand(input_buffer.buffer, table)) {
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

string parse_main_args(int argc, char** argv) {
    if (argc < 2) {
        cerr << "Usage: db <db_filename> [--debug]" << endl;
        exit(EXIT_FAILURE);
    }

    string filename = argv[1];

    for(int i = 2; i < argc; i++) {
        string arg = argv[i];
        cout << arg << endl;
        if(arg == "--debug" || arg == "-d") {
            DEBUG_MODE = true;
            cout << "Debug mode enabled." << endl;
        }
    }

    return filename;
}

int main(int argc, char** argv) {
    string filename = parse_main_args(argc, argv);
    repl_loop(filename);
    
    return 0;
}