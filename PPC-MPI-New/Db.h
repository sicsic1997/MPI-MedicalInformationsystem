#pragma once
#include "PatientHistory.h"

class Db
{
private:
	std::vector<std::shared_ptr<PatientHistory>> patientsHistory;
	void populateDb();
public:
	std::shared_ptr<PatientHistory> getPatientHistoryByName(std::string name);
	std::vector<std::shared_ptr<PatientHistory>> getPatientsHistory();
	std::string getPatientNames();
	bool updatePatientHistory(std::string name, std::string details);
	Db();
};

