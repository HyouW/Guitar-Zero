#include "gpiolib_addr.h"
#include "gpiolib_reg.h"
#include "gpiolib_reg.c"

#include <stdint.h>
#include <stdio.h>		//for the printf() function
#include <fcntl.h>
#include <linux/watchdog.h> 	//needed for the watchdog specific constants
#include <unistd.h> 		//needed for sleep
#include <sys/ioctl.h> 		//needed for the ioctl function
#include <stdlib.h> 		//for atoi
#include <time.h> 		//for time_t and the time() function
#include <sys/time.h>           //for gettimeofday()
#include <ao/ao.h>
#include <mpg123.h>
#include <math.h>

#define BITS 8

typedef int bool; //allows us to use boolean variables in the C programming language
#define true 1 //defines true
#define false 0 //defines false
#define DIODEPIN 22 //defines the gpio pin used for the input from the diode pin

/*A print message macro used to write lines to the log file to condense 
later code
*/
#define PRINT_MSG(file, time, programName, str) \
	do{ \
			getTime(time); \
			fprintf(file, "%s : %s : %s\n\n", time, programName, str); \
			fflush(file); \
	}while(0)

/*A structure that represents a single row. It contains the integer statuses that 
represent the states of the LEDs in that row
*/
struct row
{
	int status1; 
	int status2;
	int status3;
};

/*A structure for the pins of the buttons that we have. Each integer corresponds
to the pin number of a button. This structure is used to simplify the amount of
parameters we pass into our hardware controlling fucntions
*/
struct buttons
{
	int b1;
	int b2;
	int b3;
};
/*A structure to hold the pin numbers for all the LEDs that we have. The variable
names follow the following convention: ledxy is the led in row x and column y. This
structure is used to simplify passing pin numbers into later functions
*/
struct ledPins
{
	int led11;
	int led12;
	int led13;
	int led21;
	int led22;
	int led23;
};

/*
 * Read an numeric param from the file and assign it to the integer param array (after converting from char to int)
 */
void readIntParam(char* buffer, int* storageIntArr, int* i, int* intInput)
{
	//Keeps looping while the character in the line is not NULL
	while(buffer[*i] != 0)
	{
		//If the character is a number from 0 to 9
		if(buffer[*i] >= '0' && buffer[*i] <= '9')
		{
			//Move the previous digits up one position and add the new digit
			//(Assigned to an element in the int param array)
			storageIntArr[*intInput] = (storageIntArr[*intInput]*10) + (buffer[*i] - '0');
		}
		//increment the character counter for the line
		(*i)++;
	}
	//increment the integer index for the int param array (for a new int param)
	(*intInput)++;
}

/*
 * Reads and returns the size of the path parameter
 */
int getSizeOfPathStr(char* buffer, int *i)
{
	//Copy the value of the deferenced i pointer (we don't want to change *i's value)
	int tempIndex = *i;

	//Int variable to hold the size of the path (will be the return value)
	int size = 0;
	//Loop runs while the character is not a newline or null
	while(buffer[tempIndex] != 0  && buffer[tempIndex] != '\n')
	{
		//If the characters after the equal sign are not spaces, then it will add that character to the string
		if(buffer[tempIndex] != ' ')
		{
			//Increment i to the first letter of the path 
			//(usually there is one space so tempIndex is updated to the first character but *i is not)
			if (size == 0)
			{
				(*i)++;
			}
			//size increases for a valid character
			size++;
		}
		//increase the temp buffer index to go to the next character
		tempIndex++;
	}
	//return the size of the file path
	return size;
}

/*
 * Read a file path parameter from the config file and assign it to the str param array
 */
void readCharParam(char* buffer, char** storageStrArr, int* i, int* strInput)
{
	//get the string size
	int size = getSizeOfPathStr(buffer, i);

	int j = 0;
	//alocate sufficient memory using the size
	storageStrArr[*strInput] = (char*) calloc(size+1, sizeof(char));
	for (j = 0; j < size; j++)
	{
		//copy the param over into the str param array (character by character)
		storageStrArr[*strInput][j] = buffer[*i];
		(*i)++;
	}

	//Add a null terminator at the end
	storageStrArr[*strInput][j] = '\0';

	//increment the index for the str param array
	(*strInput)++;
}

/*
 * Copies the copiedStr into the vesselStr character by character
 */
void copyStr(char *vesselStr, char* copiedStr)
{
	int i = 0;
	//Copy the characters over until you reach the null terminator of the copied string
	while (copiedStr[i] != '\0')
	{
		vesselStr[i] = copiedStr[i];
		i++;
	}

	//add a null terminator to the vessel string
	vesselStr[i] = '\0';

	//free the memory allocated to the copied string (in readCharParam)
	free(copiedStr);
}

/*
 * Assigns the parameters in the config file to their respective variables 
 * (passed into the function, some by reference, some by pointer)
 */
void readConfig(FILE* configFile, char* logFileName, int* intervalTime, int* timeoutTimer, int* MAX_INTERVALS, char* badSoundPath, char* niceSoundPath)
{
	int i = 0;

	//Maximum amount of character per line
	int bufferSize = 500;

	//Buffer array for each line
	char buffer[bufferSize];

	//indexes for the int and str arrays
	int intInput = 0;
	int strInput = 0;

	//Maxmimum number of numeric parameters
	const int maxIntParams = 3;
	//Maxmimum number of path parameters
	const int maxStrParams = 3;

	//Arrays to store the int and str params
	int intParamArr[3] = {0};
	char* strParamArr[3] = {NULL};

	//Loop until the end of the file
	while(fgets(buffer, bufferSize, configFile) != NULL)
	{
		i = 0;
		//Line isn't commented out
		if(buffer[i] != '#')
		{
			//Line isn't empty
			while(buffer[i] != 0)
			{
				//An '=' char exists in the line
				if(buffer[i] == '=')
				{
					//Increment i to the ' ' after the '=' sign
					i++;
					//Integer params are first in the config file
					//Checks if the integer param array isn't filled with the integer params
					if (intInput < maxIntParams)
					{
						//Store the integer value into the array (at the designated index - intInput)
						readIntParam(buffer, intParamArr, &i, &intInput);					
					}
					//Int array is filled, now fill the str array
					else
					{
						//Store the path str in the str array at the strInput index
						readCharParam(buffer, strParamArr, &i, &strInput);
					}
				}
				//Character not '='
				else
				{
					//go to the next character in the line
					i++;
				}
			}
		}
	}

	//Assign the int values in the int param array to the variables passed in by reference
	*timeoutTimer = intParamArr[0];
	*intervalTime = intParamArr[1];
	*MAX_INTERVALS = intParamArr[2];
	
	//Copy the path values in the str param array to the strings passed by pointer (char pointer)
	copyStr(logFileName, strParamArr[0]);
	copyStr(badSoundPath, strParamArr[1]);
	copyStr(niceSoundPath, strParamArr[2]);
}

/*This function will attempt to initalize a GPIO_Handle object which would allow us to read
and write to the gpio pins on the pi. If unsuccesful the function relies on the watchdog 
restaring the program. Otherwise the pins corresponding to the leds are set to output
for later use. This function returns the initalized GPIO_Handle object
*/
GPIO_Handle initializePins (const struct ledPins LEDs)
{
	GPIO_Handle gpio; //the return variable
	gpio = gpiolib_init_gpio();
	if (gpio == NULL) //if initialization failed
	{
		while (1) //go into an infinite loop to be saved by the watchdog eventually
		{
		}
	}
	uint32_t sel_reg;
	
	sel_reg = gpiolib_read_reg (gpio, GPFSEL(LEDs.led11/10)); //register for led11
	sel_reg |= 1 << (LEDs.led11%10)*3; //set pin for led11 to output
	gpiolib_write_reg(gpio, GPFSEL(LEDs.led11/10), sel_reg);

	sel_reg = gpiolib_read_reg (gpio, GPFSEL(LEDs.led12/10)); //register for led12
	sel_reg |= 1 << (LEDs.led12%10)*3; //set pin for led12 to output
	gpiolib_write_reg(gpio, GPFSEL(LEDs.led12/10), sel_reg);

	sel_reg = gpiolib_read_reg (gpio, GPFSEL(LEDs.led13/10)); //register for led13
	sel_reg |= 1 << (LEDs.led13%10)*3; //set pin for led13 to output
	gpiolib_write_reg(gpio, GPFSEL(LEDs.led13/10), sel_reg);

	sel_reg = gpiolib_read_reg (gpio, GPFSEL(LEDs.led21/10)); //register for led21
	sel_reg |= 1 << (LEDs.led21%10)*3; //set pin for led21 to output
	gpiolib_write_reg(gpio, GPFSEL(LEDs.led21/10), sel_reg);

	sel_reg = gpiolib_read_reg (gpio, GPFSEL(LEDs.led22/10)); //register for led22
	sel_reg |= 1 << (LEDs.led22%10)*3; //set pin for led22 to output
	gpiolib_write_reg(gpio, GPFSEL(LEDs.led22/10), sel_reg);

	sel_reg = gpiolib_read_reg (gpio, GPFSEL(LEDs.led23/10)); //register for led23
	sel_reg |= 1 << (LEDs.led23%10)*3; //set pin for led23 to output
	gpiolib_write_reg(gpio, GPFSEL(LEDs.led23/10), sel_reg);
	
	sel_reg = gpiolib_read_reg (gpio, GPFSEL(1)); //sets pin 13 to ALT0, PWM0 for audio output
	sel_reg &= 0 << 9;
	sel_reg |= 1 << 11;
	gpiolib_write_reg(gpio, GPFSEL(1), sel_reg);

	system("gpio_alt -p 18 -f 5"); //use asystem command to set pin 18 to ALT5, PWM1 for audio output

	return gpio;

}

/*This function returns if any of the buttons are currently beign pressed
*/
bool anythingPressed (const struct buttons butts, const GPIO_Handle gpio) 
{
	uint32_t sel_reg;

	sel_reg = gpiolib_read_reg (gpio, GPLEV(butts.b1/32)); //reads level register
	sel_reg &= 1 << butts.b1; //checks button state
	if (!sel_reg)
		return true;

	sel_reg = gpiolib_read_reg (gpio, GPLEV(butts.b2/32)); //reads level register
	sel_reg &= 1 << butts.b2; //cehcks button state
	if (!sel_reg)
		return true;

	sel_reg = gpiolib_read_reg (gpio, GPLEV(butts.b3/32)); //reads level register
	sel_reg &= 1 << butts.b3; //checks button state
	if (!sel_reg)
		return true;
	
	return false;
}

/*This is a helper function that sets a specified pin number to on
assuming it has been set to output already
*/
void outputOn(GPIO_Handle gpio, int pinNumber)
{
	gpiolib_write_reg(gpio, GPSET(0), 1 << pinNumber);
}

/*This us a helper function that sets a specifed pin number to off
assuming it has been set to output already
*/
void outputOff(GPIO_Handle gpio, int pinNumber)
{
	gpiolib_write_reg(gpio, GPCLR(0), 1 << pinNumber);
}


/*This functions reads the row* song and sets the states of the two LED
rows to the corresponding current rows indicated by the interval counter
*/
void updateLEDs (const struct ledPins LEDs, const struct row *song, const int intervalCounter, const GPIO_Handle gpio)
{
	if (intervalCounter == 0) //first time running
	{
		if (song[0].status1)
			outputOn(gpio, LEDs.led21);
		else
			outputOff(gpio, LEDs.led21);

		if (song[0].status2)
			outputOn(gpio, LEDs.led22);
		else
			outputOff(gpio, LEDs.led22);

		if (song[0].status3)
			outputOn(gpio, LEDs.led23);
		else
			outputOff(gpio, LEDs.led23);

	}
	else 
	{
		//sets back row, which is the row the interval counter currently represents
		if (song[intervalCounter].status1)
			outputOn(gpio, LEDs.led21);
		else
			outputOff(gpio, LEDs.led21);

		if (song[intervalCounter].status2)
			outputOn(gpio, LEDs.led22);
		else
			outputOff(gpio, LEDs.led22);

		if (song[intervalCounter].status3)
			outputOn(gpio, LEDs.led23);
		else
			outputOff(gpio, LEDs.led23);
		//sets the front row, which is represented by the interval counter - 1
		if (song[intervalCounter-1].status1)
			outputOn(gpio, LEDs.led11);
		else
			outputOff(gpio, LEDs.led11);

		if (song[intervalCounter-1].status2)
			outputOn(gpio, LEDs.led12);
		else
			outputOff(gpio, LEDs.led12);

		if (song[intervalCounter-1].status3)
			outputOn(gpio, LEDs.led13);
		else
			outputOff(gpio, LEDs.led13);
	}
}

/*This functionr returns whethere or not the laser is broken
*/
bool ifStrummed (const GPIO_Handle gpio)
{
	uint32_t level_reg = gpiolib_read_reg(gpio, GPLEV(0)); //reads level register
	return !(level_reg & (1 << DIODEPIN)); //returns if it is broken
}

/*This function compares the current states of the buttons to the states that they
should be as given by the row* song and the interval counter. This function returns
true if the user was correct and false otherwise
*/
bool checkButtons (const GPIO_Handle gpio, const int intervalCounter, const struct buttons butts, const struct row *song)
{
	uint32_t lvl_reg; //unsigned int level register variable

	lvl_reg = gpiolib_read_reg (gpio, GPLEV(0));
	if(!(((lvl_reg&(1 << butts.b1)) >> butts.b1) ^ song[intervalCounter].status1)) //the button is not what it should be
		return false;

	lvl_reg = gpiolib_read_reg (gpio, GPLEV(0));
	if(!(((lvl_reg&(1 << butts.b2)) >> butts.b2) ^ song[intervalCounter].status2)) //the button is not what it should be
		return false;

	lvl_reg = gpiolib_read_reg (gpio, GPLEV(0));
	if(!(((lvl_reg&(1 << butts.b3)) >> butts.b3)^song[intervalCounter].status3)) //the button is not what it should be
		return false;
	
	return true;
}

void playSound (char* path) //plays specified sound sound
{
	/*
	function plays a mp3 file based on the filepath input
	outputs nothing 

	based on sample code provided by the libraries 
	*/
	mpg123_handle *mh;
    unsigned char *buffer;
    size_t buffer_size;
    size_t done;
    int err;

    int driver;
    ao_device *dev;

    ao_sample_format format;
    int channels, encoding;
    long rate;

    /* initializations */
    ao_initialize();
    driver = ao_default_driver_id();
    mpg123_init();
    mh = mpg123_new(NULL, &err);
    buffer_size = mpg123_outblock(mh);
    buffer = (unsigned char*) malloc(buffer_size * sizeof(unsigned char));

    /* open the file and get the decoding format */
    mpg123_open(mh, path);
    mpg123_getformat(mh, &rate, &channels, &encoding);

    /* set the output format and open the output device */
    format.bits = mpg123_encsize(encoding) * BITS;
    format.rate = rate;
    format.channels = channels;
    format.byte_format = AO_FMT_NATIVE;
    format.matrix = 0;
    dev = ao_open_live(driver, &format, NULL);

    /* decode and play */
    while (mpg123_read(mh, buffer, buffer_size, &done) == MPG123_OK)
        ao_play(dev, buffer, done);

    /* clean up */
    free(buffer);
    ao_close(dev);
    mpg123_close(mh);
    mpg123_delete(mh);
    mpg123_exit();
    ao_shutdown();

}

/*This is a help function that sets the states of all of the LEDs to on
*/
void setAllOn (const GPIO_Handle gpio, const struct ledPins LEDs)
{
	outputOn (gpio, LEDs.led11);
	outputOn (gpio, LEDs.led12);
	outputOn (gpio, LEDs.led13);
	outputOn (gpio, LEDs.led21);
	outputOn (gpio, LEDs.led22);
	outputOn (gpio, LEDs.led23);
}

/*This is a helper fucntiont hat sets the state of all LEDs to off
*/
void setAllOff (const GPIO_Handle gpio, const struct ledPins LEDs)
{
	outputOff (gpio, LEDs.led11);
	outputOff (gpio, LEDs.led12);
	outputOff (gpio, LEDs.led13);
	outputOff (gpio, LEDs.led21);
	outputOff (gpio, LEDs.led22);
	outputOff (gpio, LEDs.led23);	
}

/* THis function flashes all of the LEDs to indicate to the user that the
song has finished
*/
void finishedSong (const GPIO_Handle gpio, const struct ledPins LEDs)
{
	printf("Song is finished. Restarting in 10 seconds\n");
	fflush(stdout);

	setAllOn (gpio, LEDs);
	usleep (1000000);
	setAllOff (gpio, LEDs);
	usleep (1000000);
	setAllOn (gpio, LEDs);
	usleep (1000000);
	setAllOff (gpio, LEDs);
	usleep(1000000);
	setAllOn (gpio, LEDs);
	usleep (1000000);
	setAllOff (gpio, LEDs);
	usleep (1000000);
	setAllOn (gpio, LEDs);
	usleep (1000000);
	setAllOff (gpio, LEDs);
}

//This function will get the current time using the gettimeofday function
void getTime(char* buffer)
{
	//Create a timeval struct named tv
  	struct timeval tv;

	//Create a time_t variable named curtime
  	time_t curtime;


	//Get the current time and store it in the tv struct
  	gettimeofday(&tv, NULL); 

	//Set curtime to be equal to the number of seconds in tv
  	curtime=tv.tv_sec;

	//This will set buffer to be equal to a string that in
	//equivalent to the current date, in a month, day, year and
	//the current time in 24 hour notation.
  	strftime(buffer,30,"%m-%d-%Y  %T.",localtime(&curtime));
}

/*
 * Return the most recent score in the score log file
 */
int getPreviousScore(char* scoreFileName)
{
	FILE *scoreFile;
	//Length of the score, leading zeroes and null character included
	const int scoreLength = 2 + 1;

	//Int variable that holds the previous score: it's the return value
	int score = 0;

	//Index to loop through the previous score in the file
	int i = 0;

	//Open the score file in read mode
	scoreFile = fopen(scoreFileName, "r");
	//Direct the file pointer to the first leading 0
	fseek(scoreFile, -scoreLength - 1, SEEK_END); 
	//File exists
	if (scoreFile) 
	{
		//loop 2 times only
		while (i < 2)
		{
			//Convert score from char to int
			score = (score*10) + (getc(scoreFile) - '0');
			i++;
		}
		//close the score file
		fclose(scoreFile);
	}

	//return the previous score
	return score;
}

/*
 * Assigns a song with a set difficulty for the user to play based on their most recent score in the score log
 */
void assignSong (struct row* const song, char* scoreFileName, const int MAX_INTERVALS)
{
	//Get the most recent score
	int score = getPreviousScore(scoreFileName);

	//Establish 2 thresholds as a percentage of the max interval
	const int threshold1 = MAX_INTERVALS/3;
	const int threshold2 = 2*threshold1;

	FILE* songFile;

	//Output previous score for user to see
	printf("Previous Score: %d\n", score);

	//If the score is below 33% of the MAX_INTERVAL size, select the easy song
	if (score < threshold1)
	{
		printf("Currently Playing: Easy Song\n");
		fflush(stdout);
		songFile = fopen("/home/pi/easySong.log", "r");
	}
	//Score is above 33% and below 66%, select the medium song
	else if (score >= threshold1 && score < threshold2)
	{
		printf("Currently Playing: Medium Song\n");
		fflush(stdout);
		songFile = fopen("/home/pi/medSong.log", "r");
	}
	//Score is above 66%, select the hard song
	else
	{
		printf("Currently Playing: Hard Song\n");
		fflush(stdout);
		songFile = fopen("/home/pi/hardSong.log", "r");
	}

	int i = 0;

	//Number of character per line in the song files
	int bufferSize = 7;

	//Char array to hold each line
	char buffer[bufferSize];

	//counts the number of intervals
	int intervalCount = 0;

	//holds the desired statuses of the buttons as seen in the song
	int statusArr[3] = {0};

	//Current interval is less than the max interval and the buffer is not at the end of the file
	for (intervalCount = 0; (intervalCount < MAX_INTERVALS && fgets(buffer, bufferSize, songFile) != NULL); intervalCount++)
	{
		i = 0;
		for (int j = 0; j < 6; j += 2)
		{
			//Assign the statuses to the status array
			statusArr[i] = buffer[j] - '0';
			i++;		
		}
		
		//Assign the statuses to the status fields in the row struct (which is in an array for row structs)
		song[intervalCount] = (struct row) {.status1 = statusArr[0], .status2 = statusArr[1], .status3 = statusArr[2]};
	}
}

/*
 * Update the score log file with the new score
 */
void updateScore (char* scoreFileName, const int score, const char* programName, char* time) //update the log file
{
	//Check if the score is invalid
	if (score < 0 || score > 30)
	{
		//Warn the user and exit the function
		perror("Invalid Score");
		return;
	}

	//Number of digits
	int n = log10(score) + 1;
	//Hold a temp score for to convert from int to str
	int tempScore = score;
	//Holds the str of the score
	char scoreStr[3];
	//Add a null terminator
	scoreStr[2] = '\0';
	for (int i = 1; i >= 0; i--)
	{
		//If the current index is outside the number of digits (leading 0s), put a '0' char in front
		if (i < 2 - n)
		{
			scoreStr[i] = '0';
		}
		//Current index is inside the number of digits (actual score value)
		else
		{
			//Assign the digit to the score str
			scoreStr[i] = tempScore % 10 + '0';
			//Divide by 10 (integer division: no decimals), to get the next digit
			tempScore /= 10;
		}
	}
	//Open the score file
	FILE* scoreFile;
	scoreFile = fopen("/home/pi/score.log", "a");
	//Log the score to the score log file
	PRINT_MSG(scoreFile, time, programName, scoreStr);
	//Close the score log file
	fclose(scoreFile);
}

int main (const int argc, const char* const argv[])
{

	//Create a string that contains the program name
	const char* argName = argv[0];

	//These variables will be used to count how long the name of the program is
	int i = 0;
	int namelength = 0;

	while(argName[i] != 0)
	{
		namelength++;
		i++;
	} 

	char programName[namelength];

	i = 0;

	//Copy the name of the program without the ./ at the start
	//of argv[0]
	while(argName[i + 2] != 0)
	{
		programName[i] = argName[i + 2];
		i++;
	}

	char logTime[30];

	FILE* configFile;
	configFile = fopen("/home/pi/GuitarZero.cfg", "r");

	if (!configFile)
	{
		perror("The config file could not be opened");
	}

	int INTERVAL_TIME; //The time between intervals excluding sound time
	int timeoutTimer; //The time until the watchdog will reset the program
	int MAX_INTERVALS; //The maximum amount of intervals that will be run specifed by the config file
	char logFileName[50]; //The name of the log file specificed by the config file
	char badSoundPath[50]; //The name of the error sound file specified by the config sound
	char niceSoundPath[50]; //The name of the nice sound file specified by the config sound

	//Read the config file and assign values to the passed variables
	readConfig(configFile, logFileName, &INTERVAL_TIME, &timeoutTimer, &MAX_INTERVALS, badSoundPath, niceSoundPath);

	//close the config file
	fclose(configFile);
	
	//Open the log file
	FILE *logFile;
	logFile = fopen(logFileName, "a");

	//Log the config file event
	PRINT_MSG(logFile, logTime, argv[0], "Configuration file read");

	//This variable will be used to access the /dev/watchdog file, similar to how
	//the GPIO_Handle works
	/*int watchdog;

	//We use the open function here to open the /dev/watchdog file. If it does
	//not open, then we output an error message. We do not use fopen() because we
	//do not want to create a file if it doesn't exist
	if ((watchdog = open("/dev/watchdog", O_RDWR | O_NOCTTY)) < 0) {
		printf("Error: Couldn't open watchdog device! %d\n", watchdog);
		return -1;
	} 
	//Log that the watchdog file has been opened
	PRINT_MSG(logFile, logTime, programName, "The Watchdog file has been opened\n\n");

	//This line uses the ioctl function to set the time limit of the watchdog
	//timer to 15 seconds. The time limit can not be set higher that 15 seconds
	//so please make a note of that when creating your own programs.
	//If we try to set it to any value greater than 15, then it will reject that
	//value and continue to use the previously set time limit
	ioctl(watchdog, WDIOC_SETTIMEOUT, &timeoutTimer);
	
	//Log that the Watchdog time limit has been set
	PRINT_MSG(logFile, logTime, programName, "The Watchdog time limit has been set\n\n");

	//The value of timeout will be changed to whatever the current time limit of the
	//watchdog timer is
	ioctl(watchdog, WDIOC_GETTIMEOUT, &timeoutTimer);

	//This print statement will confirm to us if the time limit has been properly
	//changed. The \n will create a newline character similar to what endl does.
	printf("The watchdog timeout is %d seconds.\n\n", timeoutTimer);*/

	//sets up the button structure, put what pins you want to use for the buttons
	struct buttons butts;
	butts.b1 = 17;
	butts.b2 = 10;
	butts.b3 = 11;

	//sets up the LED structure, put whatever pins you want for the LEDs
	struct ledPins LEDs;
	LEDs.led11 = 4;
	LEDs.led12 = 5;
	LEDs.led13 = 6;
	LEDs.led21 = 7;
	LEDs.led22 = 8;
	LEDs.led23 = 9;

	struct row song[60]; //initializes the song

	//User always starts with the easy song upon boot
	updateScore("‪/home/pi/score.log", 0, programName, logTime);

	while (1) //runs the program infinitely
	{
		//initializes the pins
		GPIO_Handle gpio = initializePins(LEDs);

		//Log pin initialization
		PRINT_MSG(logFile, logTime, argv[0], "GPIO pins initialized");

		//Assign a song file based on the score (should be easySong since a score of 0 was assigned befoe the while loop)
		assignSong(song, "/home/pi/score.log", MAX_INTERVALS); //assign a song ready to be played based on previous scores, uses the log file

		//Turn off all lecs
		setAllOff(gpio, LEDs);

		logFile = fopen(logFileName, "a"); //log file is opened and ready to be written to
		PRINT_MSG(logFile, logTime, argv[0], "Waiting for Input");
		while (anythingPressed(butts, gpio)) //program does nothing until a button, any button is pressed
		{
			//This ioctl call will write to the watchdog file and prevent 
			//the system from rebooting. It does this every 2 seconds, so 
			//setting the watchdog timer lower than this will cause the timer
			//to reset the Pi after 1 second
			/*ioctl(watchdog, WDIOC_KEEPALIVE, 0);
			//Log that the Watchdog was kicked
			PRINT_MSG(logFile, logTime, programName, "The Watchdog was kicked\n\n");*/

			usleep(500000);
		}

		int intervalCounter = 0; //counter used to run through song during the "song"
		updateLEDs (LEDs, song, intervalCounter, gpio);
		int correct = 0; //counts how many rows they got correct for highscore purposes
		bool gotItRight = false; //used to determine outcome at the end of the time interval
		time_t currentTime = time(NULL); //sets the currentTime of the program

		PRINT_MSG(logFile, logTime, argv[0], "Game Comencing");

		while (intervalCounter < MAX_INTERVALS) //loops until song is over
		{

			usleep(3000);

			if (ifStrummed(gpio)) //they strummed (broke) the laser, so we need to check buttons
			{
				PRINT_MSG(logFile, logTime, argv[0], "Laser Strummed");
				if (intervalCounter > 0 && checkButtons(gpio, intervalCounter-1, butts, song)) //check if they are correct
					gotItRight = true; //they were correct
			}
			if (time(NULL)-currentTime >= INTERVAL_TIME) //if the interval is over
			{		
				/*ioctl(watchdog, WDIOC_KEEPALIVE, 0);
				PRINT_MSG(logFile, logTime, programName, "The Watchdog was kicked\n\n");*/

				if (gotItRight) //if they were correct
				{
					PRINT_MSG(logFile, logTime, argv[0], "Correct Response");
					correct++; //increase their score
					playSound(niceSoundPath); //play a rewarding sound
				}
				else //they were wrong
				{
					PRINT_MSG(logFile, logTime, argv[0], "Incorrect Response");
					if (intervalCounter > 0) //if this is not the first interval, since there is a grace period
						playSound(badSoundPath); //play that bad sound
				}
				gotItRight = false; //set false for next interval
				intervalCounter++; //increase the row that we are on
				updateLEDs (LEDs, song, intervalCounter, gpio); //update the LEDs
				currentTime = time(NULL); //sets new current time
			}

			
		}	
	updateScore("‪/home/pi/score.log", correct, programName, logTime);
	PRINT_MSG(logFile, logTime, programName, "Song completed and score updated");
	printf("Your score is: %d\n", correct);
	fflush(stdout);
	finishedSong(gpio, LEDs); //flashes all LEDs too indicate song is over

	//Writing a V to the watchdog file will disable to watchdog and prevent it from
	//resetting the system
	/*write(watchdog, "V", 1);
	//Log that the Watchdog was disabled
	PRINT_MSG(logFile, logTime, programName, "The Watchdog was disabled\n\n");

	//Close the watchdog file so that it is not accidentally tampered with
	close(watchdog);
	//Log that the Watchdog was closed
	PRINT_MSG(logFile, logTime, programName, "The Watchdog was closed\n\n");*/

	//Free the gpio pins
	gpiolib_free_gpio(gpio);
	//Log that the GPIO pins were freed
	PRINT_MSG(logFile, logTime, programName, "The GPIO pins have been freed\n\n");
	
	fclose(logFile);
	usleep(5000000); //dramatic pause before the entire thing happens again
	}
	//Return to end the program
	return 0;
}