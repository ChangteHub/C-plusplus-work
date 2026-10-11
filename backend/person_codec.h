#ifndef PERSON_CODEC_H
#define PERSON_CODEC_H
#include "json.hpp"
#include "person.h"
#include <memory>
nlohmann::json personJson(const Person &person);
std::unique_ptr<Person> parsePerson(const std::string &raw);
std::string textField(const nlohmann::json &body, const char *key, bool optional = false);
#endif
