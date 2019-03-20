


native:
	g++ main.cpp -lopenal -o test

web:
	em++ main.cpp -g -lopenal --embed-file Bburg1_2.mid.wav -s ALLOW_MEMORY_GROWTH=1 -o test.html