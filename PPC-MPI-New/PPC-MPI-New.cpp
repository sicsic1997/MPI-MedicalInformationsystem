#include "stdafx.h"
#include "mpi.h"

#define NORMAL_MESSAGE_TAG 1
#define GLOBAL_EXIT_TAG 2
#define SESSION_CLOSE_MESSAGE_TAG 3

std::mutex loggerMtx;
std::mutex sessionCloseMessageMtx;
std::mutex globalExitMessageMtx;

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
}

template <class T>
void logStdout(int rank, T message)
{
	// Critical section
	loggerMtx.lock();
	std::cout << "Process " << rank << ": " << message << "\n";
	loggerMtx.unlock();
}

void handleSessionCloseMessages(std::vector<bool> &sessionAllocated, int &numberOfFreeSessions, bool &shouldListen)
{
	while (shouldListen) {

		int messageReceived = false;
		MPI_Status probestatus;
		MPI_Iprobe(MPI_ANY_SOURCE, SESSION_CLOSE_MESSAGE_TAG, MPI_COMM_WORLD, &messageReceived, &probestatus);

		if (messageReceived) {

			// This waits for the message to arrive
			int sessionId = -1;
			MPI_Status dummyStatus;
			MPI_Recv(&sessionId, 1, MPI_INT, MPI_ANY_SOURCE, SESSION_CLOSE_MESSAGE_TAG, MPI_COMM_WORLD, &dummyStatus);

			// Critical section since main thread may also access these resources
			sessionCloseMessageMtx.lock();
			sessionAllocated[sessionId] = false;
			numberOfFreeSessions++;
			sessionCloseMessageMtx.unlock();

			logStdout(0 /*rank*/, "Session " + std::to_string(sessionId) + " closed.");

		}
	}
}

void handleGlobalExitMessage(int rank, bool &isProcessActive, bool &shouldContinue, FILE* in, FILE* out)
{
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
		std::thread handleSessionCloseMessagesThread(handleSessionCloseMessages, std::ref(sessionAllocated), std::ref(numberOfFreeSessions), std::ref(shouldContinue));

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
						MPI_Request mpiRequest;
						MPI_Isend(&exitMessage, 1, MPI_INT, i, GLOBAL_EXIT_TAG, MPI_COMM_WORLD, &mpiRequest);
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
	
		handleSessionCloseMessagesThread.join();
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
					if (in != nullptr && out != nullptr)
					{
						fclose(in);
						fclose(out);
					}
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
				std::cin >> userCommand;
				switch (userCommand)
				{
				case 1:
				{
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
				default:
				{
					logStdout(rank, "Invalid choice");
				}
				}
			}
		}
	
		handleGlobalExitMessageThread.join();
	}

	MPI_Finalize();
}

