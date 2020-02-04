#include "stdafx.h"
#include "PatientHistory.h"

string PatientHistory::getName() {
	return name;
}

vector<pair<time_t, string>> PatientHistory::getConsultations() {
	return consultations;
}

PatientHistory::PatientHistory(string inputName)
{
	name = inputName;
}

void PatientHistory::addConsultation(pair<time_t, string> time_diagnosis)
{
	consultations.push_back(time_diagnosis);
}

PatientHistory::PatientHistory()
{
}


PatientHistory::~PatientHistory()
{
}
