describe 'database' do
  def clean_db_file(filename="testdb.db")
    system("make clear file=#{filename}")
  end

  # before each test, clean the db file
  before do
    clean_db_file()
  end
    
  # Initial binary build of db
  before(:all) do
      def build_db()
          system("make")
      end

      build_db()
  end

  # Clean the binary used for the testing
  after(:all) do
      def clean_db()
          system("make clean")
      end
      clean_db()
      clean_db_file()
  end

  def run_script(commands)
    raw_output = nil
    IO.popen("./db.exe testdb.db", "r+") do |pipe|
      commands.each do |command|
        pipe.puts command
      end

      pipe.close_write

      # Read entire output
      raw_output = pipe.gets(nil)
    end

    raw_output.split("\n")
  end

  it 'inserts and retrieves a row' do
    result = run_script([
      "insert 1 user1 user1@example.com",
      "select",
      ".exit",
    ])
    
    puts result
    # ignore the 1st element of result as that is print of running binary - './/db.exe'
    # result.shift()

    expect(result).to match_array([
      "> Row inserted successfully.",
      "> [SELECT] (1 user1 user1@example.com)",
      "Returned 1 rows.",
      "> Encountered exit, exiting..."
    ])
  end

  it "Allows row insertions till max allowed limit, post which it should throw an error" do
    TABLE_MAX_ROWS = 1300 + 1
    
    script = (1..TABLE_MAX_ROWS).map do |i|
      "insert #{i} user#{i} user#{i}@email.com"
    end
    script << ".exit"

    result = run_script(script)
    expect(result[-3]).to eq("> Row inserted successfully.")
    expect(result[-2]).to eq("> [ERROR] Table is full, cannot insert the row")
  end

  it "Allows string columns till they don't exceed max length" do
    username = "a" * 32
    email = "a" * 255
    row = "1 #{username} #{email}"
    insert = "insert #{row}"

    script = [
      insert,
      "select",
      ".exit"
    ]

    result = run_script(script)
    
    expect(result).to match_array([
      "> Row inserted successfully.",
      "> [SELECT] (#{row})",
      "Returned 1 rows.",
      "> Encountered exit, exiting..."
    ])
  end

  it "Strings longer than allowed length not allowed, it should throw an error" do
    username = "a" * 33
    email = "a" * 256
    insert = "insert 1 #{username} #{email}"

    script = [
      insert,
      "select",
      ".exit"
    ]

    result = run_script(script)
    
    expect(result).to match_array([
      "> Token too long: #{insert}",
      "> Returned 0 rows.",
      "> Encountered exit, exiting..."
    ])
  end

  it "Negative Id not allowed, it should throw an error" do
    insert = "insert -1 user user@email.com"
    script = [
      insert,
      "select",
      ".exit"
    ]

    result = run_script(script)
    
    expect(result).to match_array([
      "> Negative token found: #{insert}",
      "> Returned 0 rows.",
      "> Encountered exit, exiting..."
    ])
  end

  it 'Data is persisted even after DB is started again' do
    # insert a row and confirm, then exit
    result = run_script([
      "insert 1 user1 user1@example.com",
      ".exit",
    ])

    expect(result).to match_array([
      "> Row inserted successfully.",
      "> Encountered exit, exiting..."
    ])

    # open a new connection and check if the row is still there
    result = run_script([
      "select",
      ".exit",
    ])

    expect(result).to match_array([
      "> [SELECT] (1 user1 user1@example.com)",
      "Returned 1 rows.",
      "> Encountered exit, exiting..."
    ])

  end
end
  