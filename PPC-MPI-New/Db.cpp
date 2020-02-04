#include "stdafx.h"
#include "Db.h"
#include "PatientHistory.h"


std::shared_ptr<PatientHistory> Db::getPatientHistoryByName(std::string name) 
{
	for (int i = 0; i < patientsHistory.size(); i++) {
		if (!patientsHistory[i]->getName().compare(name)) {
			return patientsHistory[i];
		}
	}
	return nullptr;
}

std::vector<std::shared_ptr<PatientHistory>> Db::getPatientsHistory() 
{
	return this->patientsHistory;
}

std::string Db::getPatientNames() 
{
	std::string names = "Patient names are: \n";
	for (int i = 0; i < patientsHistory.size(); i++) {
		names = names + "\t" + ". " + patientsHistory[i]->getName() + "\n";
	}
	return names;
}


bool Db::updatePatientHistory(std::string name, std::string details) 
{
	std::shared_ptr<PatientHistory> ph = getPatientHistoryByName(name);
	if (ph == nullptr) {
		return false;
	}
	ph->addConsultation(make_pair(time(0), details));
}

void Db::populateDb() 
{
	std::shared_ptr<PatientHistory> ph1 = std::make_shared<PatientHistory>("Andrei");
	std::shared_ptr<PatientHistory> ph2 = std::make_shared<PatientHistory>("Marian");
	std::shared_ptr<PatientHistory> ph3 = std::make_shared<PatientHistory>("Andreea");

	patientsHistory.push_back(ph1);
	patientsHistory.push_back(ph2);
	patientsHistory.push_back(ph3);
	updatePatientHistory("Andrei", "Consult fara mentiuni extraordinare");
	updatePatientHistory("Andrei", "Faringo-amigdalita");
	updatePatientHistory("Andreea", "Primul consult a decurs ok");
	updatePatientHistory("Andreea", "Fara boli cronice in familie");
	updatePatientHistory("Andreea", "Reclama dureri abdominale. Posibila apendicita");
	updatePatientHistory("Andreea", "Durerile persista. Trebuie sa mearga imediat la urgente.");
}

Db::Db()
{
	populateDb();
}
