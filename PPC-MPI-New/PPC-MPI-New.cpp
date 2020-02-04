#include "stdafx.h"
#include "mpi.h"
#include "Db.h"

#define NORMAL_MESSAGE_TAG 1
#define GLOBAL_EXIT_TAG 1 << 1
#define SESSION_CLOSE_MESSAGE_TAG  1 << 2
#define GET_PATIENTS_TAG 1 << 3
#define GET_PATIENT_HISTORY 1 << 4
#define UPDATE_PATIENT_HISTORY 1 << 5

#define MAX_CHARACTERS_IN_MESSAGE 2000

std::mutex loggerMtx;
std::mutex sessionCloseMessageMtx;
std::mutex globalExitMessageMtx;
std::mutex actionMutex;


void printMenu() 
{
	for (int i = 0; i <= 5; i++) {
		std::cout << "\n";
	}
	std::cout << "-----------------------------------------\n";
	std::cout << "Choose your option:\n";
	std::cout << "\t 1. Connect to new session \n";
	std::cout << "\t 2. Terminate application \n";
}

void printUserMenu()
{
	std::cout << "You are logged into a session. What do you want to do?\n";
	std::cout << "\t 1. Close session \n";
	std::cout << "\t 2. List patients \n";
	std::cout << "\t 3. Get patient History for patient \n";
	std::cout << "\t 4. Update patient history \n";
}

template <class T>
void logStdout(int rank, T message)
{
	// Critical section
	loggerMtx.lock();
	std::cout << "Process " << rank << ": " << message << "\n";
	loggerMtx.unlock();
}

void handleSessionMessages(std::vector<bool> &sessionAllocated, int &numberOfFreeSessions, bool &shouldListen, std::unique_ptr<Db> dataBase)
{
	while (shouldListen) {

		int messageReceived = false;
		MPI_Status probestatus;
		MPI_Iprobe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &messageReceived, &probestatus);

		if (messageReceived)
		{
			switch (probestatus.MPI_TAG)
			{
			case SESSION_CLOSE_MESSAGE_TAG:
			{
				// This waits for the message to arrive
				int sessionId = -1;
				MPI_Status status;
				MPI_Recv(&sessionId, 1, MPI_INT, probestatus.MPI_SOURCE, SESSION_CLOSE_MESSAGE_TAG, MPI_COMM_WORLD, &status);

				// Critical section since main thread may also access these resources
				sessionCloseMessageMtx.lock();
				sessionAllocated[sessionId] = false;
				numberOfFreeSessions++;
				sessionCloseMessageMtx.unlock();

				logStdout(0 /*rank*/, "Session " + std::to_string(sessionId) + " closed.");
				break;
			}
			case GET_PATIENTS_TAG:
			{
				int dummyData = -1;
				MPI_Status status;
				MPI_Recv(&dummyData, 1, MPI_INT, probestatus.MPI_SOURCE, GET_PATIENTS_TAG, MPI_COMM_WORLD, &status);

				// Having a single thread for accessing data prevents daca race phenomenas
				std::string patientNames = dataBase->getPatientNames();

				char *message = new char[patientNames.length() + 1];
				std::strcpy(message, patientNames.c_str());
				MPI_Send(message, patientNames.length(), MPI_CHARACTER, status.MPI_SOURCE, GET_PATIENTS_TAG, MPI_COMM_WORLD);

				logStdout(0 /*rank*/, "Session " + std::to_string(status.MPI_SOURCE) + " asked for patient names.");
				break;
			}
			case GET_PATIENT_HISTORY:
			{
				std::string patientName;
				MPI_Status status;
				char *buff = new char[MAX_CHARACTERS_IN_MESSAGE]();
				MPI_Recv(buff, MAX_CHARACTERS_IN_MESSAGE - 1, MPI_CHARACTER, probestatus.MPI_SOURCE, GET_PATIENT_HISTORY, MPI_COMM_WORLD, &status);
				patientName = buff;
				delete buff;

				// Having a single thread for accessing data prevents daca race phenomenas
				std::string patientHistory = "History of " + patientName + "\n";
				std::shared_ptr<PatientHistory> patientHstory = dataBase->getPatientHistoryByName(patientName);
				if(patientHstory != nullptr) 
				{
					vector<pair<time_t, string>> consultations = patientHstory->getConsultations();
					for (auto time_diagnosis : consultations)
					{
						patientHistory += "\t" + time_diagnosis.second + "\n";
					}
				}
				else
				{
					patientHistory = "No pacient with this name found";
				}
				

				char *message = new char[patientHistory.length() + 1];
				std::strcpy(message, patientHistory.c_str());
				MPI_Send(message, patientHistory.length(), MPI_CHARACTER, status.MPI_SOURCE, GET_PATIENT_HISTORY, MPI_COMM_WORLD);

				logStdout(0 /*rank*/, "Session " + std::to_string(status.MPI_SOURCE) + " asked for patient history.");
				break;
			}
			case UPDATE_PATIENT_HISTORY:
			{
				std::string patientName;
				MPI_Status status;
				char *buff = new char[MAX_CHARACTERS_IN_MESSAGE]();
				MPI_Recv(buff, MAX_CHARACTERS_IN_MESSAGE - 1, MPI_CHARACTER, probestatus.MPI_SOURCE, UPDATE_PATIENT_HISTORY, MPI_COMM_WORLD, &status);
				patientName = buff;
				delete buff;

				std::string patientDiagnostic;
				char *buff1 = new char[MAX_CHARACTERS_IN_MESSAGE]();
				MPI_Recv(buff, MAX_CHARACTERS_IN_MESSAGE - 1, MPI_CHARACTER, probestatus.MPI_SOURCE, UPDATE_PATIENT_HISTORY, MPI_COMM_WORLD, &status);
				patientDiagnostic = buff1;
				delete buff1;


				std::shared_ptr<PatientHistory> patientHstory = dataBase->getPatientHistoryByName(patientName);
				std::string patientHistory = "History of " + patientName + "\n";
				if (patientHstory != nullptr)
				{
					// Update patient history
					dataBase->updatePatientHistory(patientName, patientDiagnostic);

					// Get new history and return it
					vector<pair<time_t, string>> consultations = patientHstory->getConsultations();
					for (auto time_diagnosis : consultations)
					{
						patientHistory += "\t" + time_diagnosis.second + "\n";
					}
				}
				else
				{
					patientHistory = "No pacient with this name found";
				}


				char *message = new char[patientHistory.length() + 1];
				std::strcpy(message, patientHistory.c_str());
				MPI_Send(message, patientHistory.length(), MPI_CHARACTER, status.MPI_SOURCE, UPDATE_PATIENT_HISTORY, MPI_COMM_WORLD);

				logStdout(0 /*rank*/, "Session " + std::to_string(status.MPI_SOURCE) + " asked for patient history.");
				break;
			}
			}
		}
	}
}

void handleGlobalExitMessage(int rank, bool &isProcessActive, bool &shouldContinue, FILE* in, FILE* out)
{

	MPI_Status exitSession;
	int exitReceived = 0;
	MPI_Iprobe(0, GLOBAL_EXIT_TAG, MPI_COMM_WORLD, &exitReceived, &exitSession);
	while (!exitReceived) {
		MPI_Iprobe(0, GLOBAL_EXIT_TAG, MPI_COMM_WORLD, &exitReceived, &exitSession);
	}

	MPI_Status dummyStatus;
	int receivedMessage = -1;
	MPI_Recv(&receivedMessage, 1, MPI_INT, 0, GLOBAL_EXIT_TAG, MPI_COMM_WORLD, &dummyStatus);

	// Critical section
	globalExitMessageMtx.lock();
	if (in != nullptr && out != nullptr)
	{
		fclose(in);
		fclose(out);
	}
	::FreeConsole();
	shouldContinue = false;
	isProcessActive = false;
	globalExitMessageMtx.unlock();
}


int main(int argc, char *argv[])
{

	const int rc = MPI_Init(&argc, &argv);
	if (rc != MPI_SUCCESS)
	{
		std::cout << "Error starting MPI program. Terminating.\n";
		MPI_Abort(MPI_COMM_WORLD, rc);
	}

	int  numtasks, rank;
	MPI_Comm_size(MPI_COMM_WORLD, &numtasks);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);


	if (numtasks < 2)
	{
		std::cout << "Error: create 2 processes for this test\n";
		MPI_Finalize();
		return -1;
	}

	if (rank == 0)
	{
		// We should have a thread for getting the user input
		// and another thread to manage session closing messages
		std::vector<bool> sessionAllocated(numtasks);
		int numberOfFreeSessions = numtasks - 1;
		bool shouldContinue = true;
		std::thread handleSessionMessagesThread(handleSessionMessages,
			std::ref(sessionAllocated),
			std::ref(numberOfFreeSessions),
			std::ref(shouldContinue),
			std::move(std::make_unique<Db>()));

		while (shouldContinue)
		{
			// Main thread responsabilities: listen to user input and create new session on request.
			printMenu();
			int userChoice = -1;

			std::cin >> userChoice;
			switch (userChoice)
			{
			case 1:
			{
				// Check if any session available
				int sessionId = -1;
				for (int i = 1; i < numtasks; i++)
				{
					if (sessionAllocated[i] == false)
					{
						sessionId = i;
						break;
					}
				}
				if (sessionId == -1)
				{
					// Prompt user no session is available
					logStdout(rank, "No session is available. Please try again later.");
					continue;
				}
				else
				{
					// Critical section when accessing sessionAllocated and numberOfFreeSession
					// Sending session init message
					sessionCloseMessageMtx.lock();
					int message = 1; // Start session
					MPI_Send(&message, 1, MPI_INT, sessionId, NORMAL_MESSAGE_TAG, MPI_COMM_WORLD);
					sessionAllocated[sessionId] = true;
					numberOfFreeSessions--;
					sessionCloseMessageMtx.unlock();
				}
				break;
			}
			case 2:
			{
				// Prompt user the application ends
				logStdout(rank, "The application will soon exit.");
				int exitMessage = 0;
				for (int i = 1; i < numtasks; i++) {
						MPI_Send(&exitMessage, 1, MPI_INT, i, GLOBAL_EXIT_TAG, MPI_COMM_WORLD);
				}
				shouldContinue = false;
				break;
			}
			default:
			{
				logStdout(rank, "Invalid choice");
			}
			}
		}
	
		handleSessionMessagesThread.join();
	}
	else
	{
		bool isProcessActive = false;
		bool shouldContinue = true;
		FILE *in = nullptr;
		FILE *out = nullptr;

		std::thread handleGlobalExitMessageThread(handleGlobalExitMessage, rank, std::ref(isProcessActive), std::ref(shouldContinue), in, out);
		while (shouldContinue) {

			if (!isProcessActive) 
			{
				MPI_Status startSession;
				int startSessionReceived = 0;
				MPI_Iprobe(0, NORMAL_MESSAGE_TAG, MPI_COMM_WORLD, &startSessionReceived, &startSession);

				if (startSessionReceived == 1)
				{
					int messageId = -1;
					MPI_Status mpiStatus;
					MPI_Recv(&messageId, 1, MPI_INT, 0, NORMAL_MESSAGE_TAG, MPI_COMM_WORLD, &mpiStatus);

					// Critical section 
					globalExitMessageMtx.lock();
					::FreeConsole();
					::AllocConsole();
					in = freopen("CONIN$", "r", stdin);
					out = freopen("CONOUT$", "w", stdout);
					isProcessActive = true;
					globalExitMessageMtx.unlock();

					logStdout(rank, "Session started");
				}

			}
			else
			{
				printUserMenu();

				int userCommand = -1;

				loggerMtx.lock();
				std::cin >> userCommand;
				loggerMtx.unlock();

				// User may have taken a while to answer
				// Let's check if the session is still up
				if(!shouldContinue) 
				{
					break;
				}

				switch (userCommand)
				{
				case 1:
				{
					// Log Out
					logStdout(rank, "Session will soon be closing");

					globalExitMessageMtx.lock();
					if (in != nullptr && out != nullptr)
					{
						fclose(in);
						fclose(out);
					}
					::FreeConsole();
					isProcessActive = false;
					globalExitMessageMtx.unlock();

					int message = rank;
					MPI_Send(&message, 1, MPI_INT, 0, SESSION_CLOSE_MESSAGE_TAG, MPI_COMM_WORLD);
					
					break;
				}
				case 2:
				{
					// Display All Patients
					int dummyMessage = -1;
					MPI_Send(&dummyMessage, 1, MPI_INT, 0, GET_PATIENTS_TAG, MPI_COMM_WORLD);

					/*MPI_Status namesSession;
					int namesReceived = 0;
					MPI_Iprobe(0, GET_PATIENTS_TAG, MPI_COMM_WORLD, &namesReceived, &namesSession);
					while (!namesReceived) {
						MPI_Iprobe(0, GET_PATIENTS_TAG, MPI_COMM_WORLD, &namesReceived, &namesSession);
					}
					*/
					std::string patientNames;
					MPI_Status status;
					char *buff = new char[MAX_CHARACTERS_IN_MESSAGE]();
					MPI_Recv(buff, MAX_CHARACTERS_IN_MESSAGE - 1, MPI_CHARACTER, 0, GET_PATIENTS_TAG, MPI_COMM_WORLD, &status);


					patientNames = buff;
					delete buff;
					logStdout(rank, patientNames);
					break;
				}
				case 3:
				{
					std::string patientName;
					std::cout << "Please give patient name: \n";
					std::cin >> patientName;
					
					// Pass patientName
					MPI_Send(&patientName, patientName.size(), MPI_CHARACTER, 0, GET_PATIENT_HISTORY, MPI_COMM_WORLD);

					// Receive patientHistory
					std::string patientHistory;
					MPI_Status status;
					char *buff = new char[MAX_CHARACTERS_IN_MESSAGE]();
					MPI_Recv(buff, MAX_CHARACTERS_IN_MESSAGE - 1, MPI_CHARACTER, 0, GET_PATIENT_HISTORY, MPI_COMM_WORLD, &status);

					patientHistory = buff;
					delete buff;
					logStdout(rank, patientHistory);
					break;
				}
				case 4:
				{
					std::string patientName;
					std::string patientDiagnostic;
					std::cout << "Please give patient name: \n";
					std::cin >> patientName;
					std::cout << "Please give patient diagnostic: \n";
					std::cin >> patientDiagnostic;

					// Pass patientName
					MPI_Send(&patientName, patientName.size(), MPI_CHARACTER, 0, UPDATE_PATIENT_HISTORY, MPI_COMM_WORLD);

					// Pass patientDiagnostic
					MPI_Send(&patientDiagnostic, patientDiagnostic.size(), MPI_CHARACTER, 0, UPDATE_PATIENT_HISTORY, MPI_COMM_WORLD);

					// Receive new patientHistory
					std::string patientHistory;
					MPI_Status status;
					char *buff = new char[MAX_CHARACTERS_IN_MESSAGE]();
					MPI_Recv(buff, MAX_CHARACTERS_IN_MESSAGE - 1, MPI_CHARACTER, 0, UPDATE_PATIENT_HISTORY, MPI_COMM_WORLD, &status);

					patientHistory = buff;
					delete buff;
					logStdout(rank, patientHistory);
					break;
				}
				default:
				{
					logStdout(rank, "Invalid choice");
				}
				}
			}
		}
	
		handleGlobalExitMessageThread.join();
	}

	::FreeConsole();
	MPI_Finalize();
	return 0;
}

