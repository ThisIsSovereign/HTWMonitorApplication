// Tarts Sensors
#include <Tarts.h>
#include <TartsStrings.h>

// MySQL
#include "mysql_connection.h"
#include "mysql_driver.h"
#include <cppconn/driver.h>
#include <cppconn/exception.h>
#include <cppconn/resultset.h>
#include <cppconn/statement.h>
#include <cppconn/prepared_statement.h>
#include <HTWMonitorDB.h>

// General
#include <iostream>
#include <string>
#include <sstream>
#include <time.h>


// Tarts Hardware IDs
#define GATEWAY_ID   "T5YCDF"
#define TEMPERATURE_ID    "T5YK0X"
#define HUMIDITY_ID	 "T5YEP7"
#define WATER_ID	 "T5YEAX"

/**********************************************************************************
 *BEAGLEBONE BLACK PLATFORM-SPECIFIC DEFINITIONS
 *********************************************************************************/
#ifdef BB_BLACK_ARCH
  #define GATEWAY_CHANNELS      0xFFFFFFFF // All Channels
  #define GATEWAY_UARTNUMBER    1          //<<your UART Selection>>
  #define GATEWAY_PINACTIVITY   P9_42      //<<your Activity Pin location>>
  #define GATEWAY_PINPCTS       P8_26      //<<your PCTS Pin location>>
  #define GATEWAY_PINPRTS       P8_15      //<<your PRTS Pin location>>
  #define GATEWAY_PINRESET      P8_12      //<<your Reset Pin location>>
#endif

// Global Variables
int AppCalls = 0;   //Keyboard Read Thread callback variable
std::string currentUser = "NULL";
std::string currentLocation = "NULL";
int sensorUpdateFreq = 5;

// Get Current Date
std::string getCurrentDate() 
{
    time_t     now = time(0);
    struct tm  tstruct;
    char       buf[80];
    tstruct = *localtime(&now);
    strftime(buf, sizeof(buf), "%Y-%m-%d", &tstruct);

    return buf;
}

// Get Current Time
std::string getCurrentTime() 
{
    time_t     now = time(0);
    struct tm  tstruct;
    char       buf[80];
    tstruct = *localtime(&now);
    strftime(buf, sizeof(buf), "%X", &tstruct);

    return buf;
}

// Pull Sensor Update Frequency from Database
void pullSensorUpdateFreq()
{
	int sensorUpdateFreqDB = 0;
	
	// MySQL
	sql::mysql::MySQL_Driver *driver;
	sql::Connection *con;
	sql::PreparedStatement *prep_stmt;
	sql::ResultSet *res;
	
	driver = sql::mysql::get_mysql_driver_instance();
	con = driver->connect(DB_HOST, DB_USER, DB_PASSWORD);
	con->setSchema(DB_NAME);
		
	prep_stmt = con->prepareStatement("SELECT UpdateFrequency FROM Users WHERE UserName=(?)");
	prep_stmt->setString(1, currentUser);
	res = prep_stmt->executeQuery();
	
	while (res->next())
	{
		sensorUpdateFreqDB = res->getInt(1);
	}
	
	delete res;
	delete prep_stmt;
	delete con;
	
	if (sensorUpdateFreq != sensorUpdateFreqDB)
	{
		sensorUpdateFreq = sensorUpdateFreqDB;
	}
}

// Tarts Events

// Handle Gateway Messages
void onGatewayMessageEvent(const char* id, int stringID)
{
  printf("TARTS-GWM[%s]-%d: %s\n", id, stringID, TartsGatewayStringTable[stringID]);
  if(stringID == 2) printf("Found GW: %s\n", Tarts.FindGateway(GATEWAY_ID)->getLastUnknownID());
  if(stringID == 10) printf("ACTIVE - Channel: %d\n", Tarts.FindGateway(GATEWAY_ID)->getOperatingChannel());

}

// Handle Sensor Messages
void onSensorMessageEvent(SensorMessage* msg)
{
	std::string messageName = msg->DatumList[0].Name;
	TartsSensorBase* sensor = Tarts.FindSensor(TEMPERATURE_ID);
	
	std::cout << messageName << std::endl;
		
	// Apply Sensor Update Frequency	
	if (messageName == "TEMPERATURE")
	{
		sensor = Tarts.FindSensor(TEMPERATURE_ID);
		sensor->setReportInterval(sensorUpdateFreq);
	}
	else if (messageName == "RH")
	{
		sensor = Tarts.FindSensor(HUMIDITY_ID);
		sensor->setReportInterval(sensorUpdateFreq);
	}
	
	// MYSQL
	sql::mysql::MySQL_Driver *driver;
	sql::Connection *con;
	sql::PreparedStatement *prep_stmt;
	
	driver = sql::mysql::get_mysql_driver_instance();
	con = driver->connect(DB_HOST, DB_USER, DB_PASSWORD);
	con->setSchema(DB_NAME);
	
	// Sensor Status
	printf("TARTS-SEN[%s]: RSSI: %d dBm, Battery Voltage: %d.%02d VDC, Data: ", msg->ID, msg->RSSI, msg->BatteryVoltage/100, msg->BatteryVoltage%100);
	for(int i=0; i< msg->DatumCount; i++)
		{
			if(i != 0) printf(" || ");
			printf("%s | %s | %s", msg->DatumList[i].Name, msg->DatumList[i].Value, msg->DatumList[i].FormattedValue);
		}
	
	// Insert Data into Database
	
	// Temperature Sensor Data
	if (messageName == "TEMPERATURE")
	{
		std::stringstream converter;
		std::string tempCelsiusString = msg->DatumList[0].Value;
		double tempCelsius;
		double tempCelsiusConverted;
		double tempFahrenheit;
		
		// Convert Celsius to Fahrenheit
		converter << tempCelsiusString;
		converter >> tempCelsius;
		tempCelsiusConverted = (tempCelsius/10.0);
		tempFahrenheit = ((tempCelsiusConverted * 1.8000) + 32);
		
		// Check if Data is in a valid range
		if (tempFahrenheit < 150.0 && tempFahrenheit > -25.0)
		{
			// Insert Data into Temperature Table
			prep_stmt = con->prepareStatement("INSERT INTO Temperature(UserName, LocationName, Temperature, Date, Time) Values (?, ?, ?, ?, ?)");
			prep_stmt->setString(1, currentUser);
			prep_stmt->setString(2, currentLocation);
			prep_stmt->setDouble(3, tempFahrenheit);
			prep_stmt->setString(4, getCurrentDate());
			prep_stmt->setString(5, getCurrentTime());
			prep_stmt->execute();
			
			// Report Current Data Entry
			std::cout << std::endl <<  "Temperature: " << tempFahrenheit << "F Location: " << currentLocation << " Date: " << getCurrentDate() << " Time: " << getCurrentTime() << std::endl;
			std::cout << "Current Sensor Report Interval: " << sensor->getReportInterval() << "\n";
			
			delete prep_stmt;
			delete con;
		}
		
	}
	
	// Relative Humidity Sensor Data
	if (messageName == "RH")
	{
		std::stringstream converter;
		std::string humidityString = msg->DatumList[0].Value;
		double humidity;
	
		converter << humidityString;
		converter >> humidity;
		humidity = (humidity/100.0);
		
		// Check if Data is in a valid range
		if (humidity <= 100.0 && humidity >= 0.0)
		{
			// Insert Data into RelativeHumidity Table
			prep_stmt = con->prepareStatement("INSERT INTO RelativeHumidity(UserName, LocationName, RelativeHumidity, Date, Time) Values (?, ?, ?, ?, ?)");
			prep_stmt->setString(1, currentUser);
			prep_stmt->setString(2, currentLocation);
			prep_stmt->setDouble(3, humidity);
			prep_stmt->setString(4, getCurrentDate());
			prep_stmt->setString(5, getCurrentTime());
			prep_stmt->execute();
			
			// Report Current Data Entry
			std::cout << std::endl <<  "Relative Humidity: " << humidity << "% Location: " << currentLocation << " Date: " << getCurrentDate() << " Time: " << getCurrentTime() << std::endl;
			std::cout << "Current Sensor Report Interval: " << sensor->getReportInterval() << "\n";
			
			delete prep_stmt;
			delete con;
		}
		
	}
	
	// Water Detection Sensor Data
	
	
	// Check for new Sensor Update Frequency
	pullSensorUpdateFreq();
	
	printf("\n");
}

// Handles Account Creation
void createAccount()
{
	std::string inputUserName = "No";
	std::string inputPassword = "No";
	std::string inputEmail = "No";
	std::string verifyPassword = "No";
	std::string verifyEmail = "No";
	
	bool userNameExists = true;
	bool passwordMatches = false;
	bool emailMatches = false;
	//bool validEmail = false;
	
	// MySQL
	sql::mysql::MySQL_Driver *driver;
	sql::Connection *con;
	sql::PreparedStatement *prep_stmt;
	sql::ResultSet *res;
	
	driver = sql::mysql::get_mysql_driver_instance();
	con = driver->connect(DB_HOST, DB_USER, DB_PASSWORD);
	con->setSchema(DB_NAME);
	
	std::cout << "<<Account Creation>>\n";
	
	// Get UserName
	std::cout << "User Names must be between 3 and 30 characters long, and are case sensitive.\n";
	std::cout << "Enter a User Name:\n";
	std::cin >> inputUserName;
	
	while (userNameExists == true)
	{
		while (inputUserName.size() > 30 || inputUserName.size() < 3)
		{
			std::cout << "Enter a User Name:\n";
			std::cin.clear();
			std::cin >> inputUserName;
		}
		
		// Check Users Table for inputUserName
		prep_stmt = con->prepareStatement("SELECT UserName FROM Users WHERE UserName=(?)");
		prep_stmt->setString(1, inputUserName);
		res = prep_stmt->executeQuery();
		
		if (res->rowsCount() == 1)
		{
			inputUserName = "No";
			std::cout << ("User Name already exists!\n\n");	
		}
		else if (res->rowsCount() == 0)
		{
			userNameExists = false;
			std::cout << ("User Name is avaliable!\n\n");
		}
	}	
	
	delete res;
	delete prep_stmt;
	
	// Get Password
	std::cout << "Passwords must be between 3 and 30 characters long, and are case sensitive.\n";
	std::cout << ("Enter a Password:\n");
	std::cin >> inputPassword;
			
	while (passwordMatches == false)
	{
		while (inputPassword.size() > 30 || inputPassword.size() < 3)
		{
			std::cout << "Enter a Password:\n";
			std::cin.clear();
			std::cin >> inputPassword;
		}
		
		// Verify Password
		std::cout << "Enter the password again to verify:\n";
		std::cin >> verifyPassword;
		
		if (inputPassword == verifyPassword)
		{
			passwordMatches = true;
			std::cout << "Passwords match.\n\n";
		}
		else
		{
			inputPassword = "No";
			std::cout << "Passwords did not match, try again.\n\n";
		}
	}
	
	// Get E-mail
	std::cout << "Enter an E-mail address:\n";
	std::cin >> inputEmail;
		
	while (emailMatches == false)
	{
		while (inputEmail.size() > 254 || inputEmail.size() < 3)
		{
			std::cout << "Enter an E-mail address:\n";
			std::cin.clear();
			std::cin >> inputEmail;
		}
		
		// Verify E-mail
		std::cout << "Enter the E-mail address again to verify:\n";
		std::cin >> verifyEmail;
		
		if (inputEmail == verifyEmail)
		{
			emailMatches = true;
			std::cout << "E-mail addresses match.\n\n";
		}
		else
		{
			inputEmail = "No";
			std::cout << "E-mail addresses did not match, try again.\n\n";
		}
	}
	
	// Insert new Account into Users Table
	prep_stmt = con->prepareStatement("INSERT INTO Users(UserName, Password, Email) Values (?, ?, ?)");
	prep_stmt->setString(1, inputUserName);
	prep_stmt->setString(2, inputPassword);
	prep_stmt->setString(3, inputEmail);
	prep_stmt->execute();
	
	delete prep_stmt;
	delete con;
	
	std::cout << "New Account successfully created!\n\n";
}

// Handles User Login
void login() 
{
	std::string inputUserName = "No";
	std::string inputPassword = "No";
	
	bool userNameExists = false;
	bool correctPassword = false;
	
	// MySQL
	sql::mysql::MySQL_Driver *driver;
	sql::Connection *con;
	sql::PreparedStatement *prep_stmt;
	sql::ResultSet *res;
	
	driver = sql::mysql::get_mysql_driver_instance();
	con = driver->connect(DB_HOST, DB_USER, DB_PASSWORD);
	con->setSchema(DB_NAME);
	
	std::cout << "<<Login>>\n";
	
	// Get UserName
	std::cout << ("Enter your User Name:\n");
	std::cin >> inputUserName;
	
	while (userNameExists == false)
	{
		while (inputUserName.size() > 30 || inputUserName.size() < 3)
		{
			std::cout << ("Enter your User Name:\n");
			std::cin.clear();
			std::cin >> inputUserName;
		}
		
		// Check Users Table for inputUserName
		prep_stmt = con->prepareStatement("SELECT UserName FROM Users WHERE UserName=(?)");
		prep_stmt->setString(1, inputUserName);
		res = prep_stmt->executeQuery();
		
		if (res->rowsCount() == 1)
		{
			userNameExists = true;
			std::cout << ("User Name Exists.\n\n");	
		}
		else if (res->rowsCount() == 0)
		{
			inputUserName = "No";
			std::cout << ("User Name does not exist!\n\n");
		}
	}	
	
	delete res;
	delete prep_stmt;
	
	// Get Password
	std::cout << ("Enter your Password:\n");
	std::cin >> inputPassword;
			
	while (correctPassword == false)
	{
		while (inputPassword.size() > 30 || inputPassword.size() < 3)
		{
			std::cout << ("Enter your Password:\n");
			std::cin.clear();
			std::cin >> inputPassword;
		}
		
		// Verify Password is Correct
		prep_stmt = con->prepareStatement("SELECT Password FROM Users WHERE UserName=(?)");
		prep_stmt->setString(1, inputUserName);
		res = prep_stmt->executeQuery();
		
		while (res->next())
		{
			if (inputPassword == res->getString(1))
			{
				correctPassword = true;
				currentUser = inputUserName;
				
				std::cout << ("Correct Password.\n");
				std::cout << "Welcome, " << currentUser  << ".\n\n";
			}
			else 
			{
				inputPassword = "No";
				std::cout << ("Incorrect Password.\n\n");
			}
		}
	}
	
	delete res;
	delete prep_stmt;
	
	delete con;
}

// Handles Location Creation
void createLocation()
{
	std::string inputLocationName = "No";
	bool locationExists = true;
	
	// MySQL
	sql::mysql::MySQL_Driver *driver;
	sql::Connection *con;
	sql::PreparedStatement *prep_stmt;
	sql::ResultSet *res;
	
	driver = sql::mysql::get_mysql_driver_instance();
	con = driver->connect(DB_HOST, DB_USER, DB_PASSWORD);
	con->setSchema(DB_NAME);
	
	std::cout << "\n<<Create Location>>\n";
	
	// Get LocationName
	std::cout << "Location Names must be between 3 and 30 characters long, and are case sensitive.\n";
	std::cout << "Enter a Location Name:\n";
	std::cin >> inputLocationName;
		
	while (locationExists == true)
	{
		while (inputLocationName.size() > 30 || inputLocationName.size() < 3)
		{
			std::cout << "Enter a Location Name:\n";
			std::cin.clear();
			std::cin >> inputLocationName;
		}
		
		// Check Locations Table for inputLocationName
		prep_stmt = con->prepareStatement("SELECT LocationName FROM Locations WHERE UserName=(?) AND LocationName=(?)");
		prep_stmt->setString(1, currentUser);
		prep_stmt->setString(2, inputLocationName);
		res = prep_stmt->executeQuery();
		
		if (res->rowsCount() == 1)
		{
			inputLocationName = "No";
			std::cout << ("That Location already exists!\n\n");	
		}
		else if (res->rowsCount() == 0)
		{
			locationExists = false;
			std::cout << ("That Location is avaliable!\n\n");
		}
	}	
	
	delete res;
	delete prep_stmt;
	
	// Insert new Location into Locations Table
	prep_stmt = con->prepareStatement("INSERT INTO Locations(UserName, LocationName) Values (?, ?)");
	prep_stmt->setString(1, currentUser);
	prep_stmt->setString(2, inputLocationName);
	prep_stmt->execute();
	
	delete prep_stmt;
	delete con;
	
	std::cout << inputLocationName << " successfully created!\n\n";
}

// Handles Location Selection
void selectLocation()
{
	int locationSelection = -1;
	unsigned int input;
	
	// MySQL
	sql::mysql::MySQL_Driver *driver;
	sql::Connection *con;
	sql::PreparedStatement *prep_stmt;
	sql::ResultSet *res;
	
	driver = sql::mysql::get_mysql_driver_instance();
	con = driver->connect(DB_HOST, DB_USER, DB_PASSWORD);
	con->setSchema(DB_NAME);
	
	std::cout << "\n<<Select Location>>\n";

	// Get and Display Current User's Locations
	prep_stmt = con->prepareStatement("SELECT LocationName FROM Locations WHERE UserName=(?)");
	prep_stmt->setString(1, currentUser);
	res = prep_stmt->executeQuery();
	
	// Check if the User has any Locations saved first
	if (res->rowsCount() > 0)
	{
		std::string userLocations[res->rowsCount()];
		
		int resIndex = 0;
		
		// Store LocationNames in local Array
		while (res->next())
		{
			//std::cout << res->getString(1) << std::endl;
			userLocations[resIndex] = res->getString(1);
			resIndex++;
		}
		
		// Get User's Location Selection
		while (locationSelection < 0)
		{
			std::cout << ("Enter the number corresponding to the location you wish to select:\n");
			for (unsigned int i = 0; i < res->rowsCount(); i++)
			{
				std::cout << i+1 << ": " << userLocations[i] << std::endl;
			}
			std::cin >> input;
			
			while(std::cin.fail()) 
			{
				std::cout << ("Enter the number corresponding to the location you wish to select:\n");
				std::cin.clear();
				std::cin.ignore(256, '\n');
				std::cin >> input;
			}
			
			if (input < 1 || input > res->rowsCount())
			{
				std::cout << ("You must enter the number corresponding to the location you wish to select:\n");
			}
			else 
			{
				locationSelection = (input-1);
				currentLocation = userLocations[locationSelection];
				std::cout << "Location selected: " << userLocations[locationSelection] << ".\n\n";
			}	
		}
	}
	else
	{
		std::cout << "You have no locations, create one first!\n\n";
	}

	delete res;
	delete prep_stmt;
	
	delete con;
}

// Get Sensor Update Frequency from User
int inputSensorUpdateFreq()
{
	int inputSensorUpdateFreq = 0;
	int input = 0;
	
	std::cout << "\nSensor Update Frequency must be between 10 seconds and 60 minutes.\n";
	
	while (inputSensorUpdateFreq == 0)
	{
		std::cout << ("Enter the new Sensor Update Frequency in seconds:\n");
			std::cin >> input;
			
			while(std::cin.fail()) 
			{
				std::cout << "Sensor Update Frequency must be between 10 seconds and 60 minutes.\n";
				std::cin.clear();
				std::cin.ignore(256, '\n');
				std::cin >> input;
			}
			
			if (input < 10 || input > 3600)
			{
				std::cout << "Sensor Update Frequency must be between 10 seconds and 60 minutes.\n";
			}
			else 
			{
				inputSensorUpdateFreq = input;
			}	
	}
	
	return inputSensorUpdateFreq;
}

// Handles Init of Sensor Update Frequency of Current Session
void initSensorUpdateFreq()
{
	int sensorUpdateFreqDB = 0;
	int input = 0;
	int updateChoice = 0;
	
	// MySQL
	sql::mysql::MySQL_Driver *driver;
	sql::Connection *con;
	sql::PreparedStatement *prep_stmt;
	sql::ResultSet *res;
	
	driver = sql::mysql::get_mysql_driver_instance();
	con = driver->connect(DB_HOST, DB_USER, DB_PASSWORD);
	con->setSchema(DB_NAME);
	
	std::cout << "\n<<Sensor Update Frequency>>\n";
	
	// Get and Display Current User's Sensor Update Frequency
	prep_stmt = con->prepareStatement("SELECT UpdateFrequency FROM Users WHERE UserName=(?)");
	prep_stmt->setString(1, currentUser);
	res = prep_stmt->executeQuery();
	
	while (res->next())
	{
		std::cout << "Your saved Sensor Update Frequency is: " << res->getInt(1) << " seconds.\n";
		sensorUpdateFreqDB = res->getInt(1);
	}
	
	delete res;
	delete prep_stmt;
	
	while (updateChoice == 0)
	{
		std::cout << ("Enter:\n(1) Use Saved\n(2) Set New\n");
			std::cin >> input;
			
			while(std::cin.fail()) 
			{
				std::cout << ("You must enter:\n(1) Use Saved\n(2) Set New\n");
				std::cin.clear();
				std::cin.ignore(256, '\n');
				std::cin >> input;
			}
			
			if (input != 1 && input != 2)
			{
				std::cout << ("You must enter:\n(1) Use Saved\n(2) Set New\n");
			}
			else 
			{
				updateChoice = input;
			}	
			
			// Used Saved or Set New
			if (updateChoice == 1)
			{
				sensorUpdateFreq = sensorUpdateFreqDB;
			}
			else if (updateChoice == 2)
			{
				sensorUpdateFreq = inputSensorUpdateFreq();
				
				// Update Saved Sensor Update Frequency in DB
				prep_stmt = con->prepareStatement("UPDATE Users SET UpdateFrequency=(?) WHERE UserName=(?)");
				prep_stmt->setInt(1, sensorUpdateFreq);
				prep_stmt->setString(2, currentUser);
				prep_stmt->executeUpdate();
				
				delete prep_stmt;
			}
	}
	
	std::cout << "The Sensor Update Frequency is now: " << sensorUpdateFreq << " seconds.\n\n";
	
	delete con;
}

// Keyboard Thread 
TARTS_THREAD(AppKeyboardReadThread)
{
	char rxchar = 0;
	while(1)
	{
		std::cin >> rxchar;
		if (rxchar == 'q') 
		{
			AppCalls = 1;     
			rxchar = 0;
		}	
	}
	return 0;
}

// Initial Setup
int setup() 
{
	printf("HTW Monitor running...");

	//Prepare all the call-back functions
	Tarts.RegisterEvent_GatewayMessage(onGatewayMessageEvent);
	Tarts.RegisterEvent_SensorMessage(onSensorMessageEvent);
	printf("All Event Handlers Registered.\n");

	//Register Gateway
	#ifdef BB_BLACK_ARCH
	if(!Tarts.RegisterGateway(TartsGateway::Create(GATEWAY_ID, GATEWAY_CHANNELS, GATEWAY_UARTNUMBER, GATEWAY_PINACTIVITY, GATEWAY_PINPCTS, GATEWAY_PINPRTS, GATEWAY_PINRESET))){
	#else
	if(!Tarts.RegisterGateway(TartsGateway::Create(GATEWAY_ID)))
	{
	#endif
	printf("TARTs Gateway Registration Failed!\n");
	return 1;
	}

	//Lastly Register All Sensors
	if(!Tarts.RegisterSensor(GATEWAY_ID, TartsTemperature::Create(TEMPERATURE_ID)))
	{
		Tarts.RemoveGateway(GATEWAY_ID);
		printf("TARTs Temperature Sensor Registration Failed!\n");
		return 2;
	}
	
	if(!Tarts.RegisterSensor(GATEWAY_ID, TartsHumidity::Create(HUMIDITY_ID)))
	{
		Tarts.RemoveGateway(GATEWAY_ID);
		printf("TARTs Humidity Sensor Registration Failed!\n");
		return 3;
	}

	if(TARTS_THREADSTART(AppKeyboardReadThread) != 0)
	{
		printf("Unable to start Keyboard Read Thread!");
		Tarts.RemoveGateway(GATEWAY_ID);
		return 4;
	}

	printf("<<Press q to exit HTW Monitor.>>\n\n");
	return 0;
}

void loop() 
{   
	Tarts.Process();
	
	//// Apply Sensor Update Frequency
	//TartsSensorBase* temperatureSensor = Tarts.FindSensor(TEMPERATURE_ID);
    //temperatureSensor->setReportInterval(sensorUpdateFreq);
    
    //TartsSensorBase* humiditySensor = Tarts.FindSensor(HUMIDITY_ID);
    //humiditySensor->setReportInterval(sensorUpdateFreq);
	
	if(AppCalls == 1)
	{
		AppCalls = 0;
		Tarts.RemoveGateway(GATEWAY_ID);
		exit(0);
	}
}

//Main Application Loop
int main(void)
{
	bool loggedIn = false;
	//bool sensorUpdateSet = false;
	int loginChoice= 0;
	int locationChoice = 0;
	int input;
	
	std::cout << ("Welcome to HTW Monitor.\n");

	// Log In
	while (loggedIn == false)
	{
		// Prompt User to Login or Create a New Account
		while (loginChoice == 0)
		{
			std::cout << ("Enter:\n(1) Login\n(2) Create a New Account\n");
			std::cin >> input;
			
			while(std::cin.fail()) 
			{
				std::cout << ("You must enter (1) Login, or (2) Create a New Account.\n");
				std::cin.clear();
				std::cin.ignore(256, '\n');
				std::cin >> input;
			}
			
			if (input != 1 && input != 2)
			{
				std::cout << ("You must enter (1) Login, or (2) Create a New Account.\n");
			}
			else 
			{
				loginChoice = input;
			}	
			
			// Login or Create a New Account
			if (loginChoice == 1)
			{
				login();
				loggedIn = true;
			}
			else if (loginChoice == 2)
			{
				createAccount();
				loginChoice = 0;
			}
		}

	}
	
	// Select Location
	while (currentLocation == "NULL")
	{
		while (locationChoice == 0)
		{
			std::cout << "You must select a location you made previously, or create a new one.\n";
			std::cout << ("Enter:\n(1) Select Location\n(2) Create a new Location\n");
			std::cin >> input;
			
				while(std::cin.fail()) 
				{
					std::cout << ("You must enter:\n(1) Select Location\n(2) Create a new Location\n");
					std::cin.clear();
					std::cin.ignore(256, '\n');
					std::cin >> input;
				}
				
				if (input != 1 && input != 2)
				{
					std::cout << ("You must enter:\n(1) Select Location\n(2) Create a new Location\n");
				}
				else 
				{
					locationChoice = input;
				}	
				
				// Select Location or Create a new Location
				if (locationChoice == 1)
				{
					selectLocation();
					
					// Handle when a user has no locations to select from
					if (currentLocation == "NULL")
					{
						locationChoice = 0;
					}
				}
				else if (locationChoice == 2)
				{
					createLocation();
					
					locationChoice = 0;
				}
		}
	}
	
	// Set Sensor Update Frequency
	while (sensorUpdateFreq < 10)
	{
		initSensorUpdateFreq();
	}
	
	if(setup() != 0) 
	{
		exit(1);
	}
	while(1)
	{
		loop();
		TARTS_DELAYMS(100); //Allow System Sleep to occur
	}
}
