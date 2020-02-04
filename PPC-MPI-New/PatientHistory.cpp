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

PatientHistory::PatientHistory()
{
}


PatientHistory::~PatientHistory()
{
}
