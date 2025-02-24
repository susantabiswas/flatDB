describe 'database' do
    before(:all) do
        def build_db()
            system("make")
        end

        build_db()
    end

    after(:all) do
        def clean_db()
            system("make clean")
        end
        clean_db()
    end

    def run_script(commands)
      raw_output = nil
      IO.popen("./db.exe", "r+") do |pipe|
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
        "insert 1 user1 person1@example.com",
        "select",
        ".exit",
      ])

      puts result

      # ignore the 1st element of result as that is print of running binary - './/db.exe'
      result.shift()

      expect(result).to match_array([
        "> Row inserted successfully.",
        "> [SELECT] (1 user1 person1@example.com)",
        "> Encountered exit, exiting..."
      ])
    end
  end
  