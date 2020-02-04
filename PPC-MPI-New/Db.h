#pragma once

using namespace std;
class Db
{
private:
	vector<PatientHistory> patients;
public:
	PatientHistory getPatientByName(string name);
	void displayPatientNames();
	void displayPatientHistory(string name);
	void updatePatientHistory(string name, string details);
	void populateDb();
	Db();
	~Db();
};

