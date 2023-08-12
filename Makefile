CPP = g++ 
FLAGS = -O5 -pg -Wall -W
RM  = rm -f
EXEC = testProg1a 

OBJECTS = \
	 Main.o \
	 read1.o \

all: $(OBJECTS) compile touch 

Main.o : Main.cpp
					 $(CPP) $(FLAGS) -c Main.cpp
read1.o : read1.cpp
			   		 $(CPP) $(FLAGS) -c read1.cpp
clean:  
					 $(RM) $(OBJECTS) $(EXEC) 

compile: 
					 $(CPP) $(FLAGS) $(OBJECTS) -o $(EXEC) 

touch:
					 @echo " "
					 @echo "Compilation done successfully..................."
					 @echo " "
