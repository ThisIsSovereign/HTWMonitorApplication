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

// General
#include <iostream>
#include <string>

#define GATEWAY_ID   "T5YCDF"
#define SENSOR_ID    "T5YK0X"

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


/**********************************************************************************
 *Global Variables
 *********************************************************************************/
int AppCalls = 0;   //Keyboard Read Thread callback variable
std::string currentUser = "NULL";


/**********************************************************************************
 *Tarts Events
 *********************************************************************************/
void onGatewayMessageEvent(const char* id, int stringID)
{
  printf("TARTS-GWM[%s]-%d: %s\n", id, stringID, TartsGatewayStringTable[stringID]);
  if(stringID == 2) printf("Found GW: %s\n", Tarts.FindGateway(GATEWAY_ID)->getLastUnknownID());
  if(stringID == 10) printf("ACTIVE - Channel: %d\n", Tarts.FindGateway(GATEWAY_ID)->getOperatingChannel());

}

void onSensorMessageEvent(SensorMessage* msg)
{
  printf("TARTS-SEN[%s]: RSSI: %d dBm, Battery Voltage: %d.%02d VDC, Data: ", msg->ID, msg->RSSI, msg->BatteryVoltage/100, msg->BatteryVoltage%100);
  for(int i=0; i< msg->DatumCount; i++)
  {
    if(i != 0) printf(" || ");
    printf("%s | %s | %s", msg->DatumList[i].Name, msg->DatumList[i].Value, msg->DatumList[i].FormattedValue);
  }
  printf("\n");
}

/**********************************************************************************
 * Keyboard Thread and Arduino-like setup / loop code
 *********************************************************************************/
TARTS_THREAD(AppKeyboardReadThread)
{
  char rxchar = 0;
  while(1)
  {
      std::cin >> rxchar;
      if(rxchar == 'q') AppCalls = 1;
      rxchar = 0;
  }
  return 0; //Here to keep the function happy
}

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
  if(!Tarts.RegisterGateway(TartsGateway::Create(GATEWAY_ID))){
#endif
    printf("TARTs Gateway Registration Failed!\n");
    return 1;
  }

  //Lastly Register All Sensors
  if(!Tarts.RegisterSensor(GATEWAY_ID, TartsTemperature::Create(SENSOR_ID)))
  {
    Tarts.RemoveGateway(GATEWAY_ID);
    printf("TARTs Temperature Sensor Registration Failed!\n");
    return 2;
  }


  if(TARTS_THREADSTART(AppKeyboardReadThread) != 0)
  {
    printf("Unable to start Keyboard Read Thread!");
    Tarts.RemoveGateway(GATEWAY_ID);
    return 3;
  }

  printf("--Press q to exit HTW Monitor.--\n\n");
  return 0;
}

void loop() 
{
  Tarts.Process();
  if(AppCalls == 1)
  {
    AppCalls = 0;
    Tarts.RemoveGateway(GATEWAY_ID);
    exit(0);
  }
}

// Handles Account Creation()
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
	con = driver->connect("tcp://sovereigndb.chnv0tgw3tgj.us-east-2.rds.amazonaws.com", "thisissovereign", "bassdubstep9999");
	con->setSchema("htwmonitor");
	
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
	con = driver->connect("tcp://sovereigndb.chnv0tgw3tgj.us-east-2.rds.amazonaws.com", "thisissovereign", "bassdubstep9999");
	con->setSchema("htwmonitor");
	
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
			std::cout << ("UserName Exists.\n\n");	
		}
		else if (res->rowsCount() == 0)
		{
			inputUserName = "No";
			std::cout << ("UserName does not exist!\n\n");
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

/**********************************************************************************
 * Execution entry point for this application
 *********************************************************************************/
//Main Application Loop
int main(void)
{
	bool loggedIn = false;
	int loginChoice= 0;
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
