#pragma once
using namespace std;

class PatientHistory 
{
private:
	string name;
	vector<pair<time_t, string>> consultations;
public:
	string getName();
	vector<pair<time_t, string>> getConsultations();
	void addConsultation(pair<time_t, string> time_diagnosis);
	PatientHistory(string inputName);
	PatientHistory();
	~PatientHistory();
};