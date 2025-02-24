db: db.cpp
	g++ db.cpp -o db

run: db
	./db

test: db
	bundle exec rspec