// Copyright (c) 2020 Aaron Alberg. All rights reserved.


#include <homefinder/engine.h>


using std::string;
using std::vector;
using nlohmann::json;

const string kNumbeoUrl =
    "http://www.numbeo.com:8008/api/indices?api_key=hbtoxypuja22wp&query=";

namespace homefinder {

Engine::Engine() = default;

homefinder::City Engine::FindIdealCity(const vector<double>& responses,
                                       const vector<homefinder::City>& cities) {
  narrowed_list_.clear();
  if (responses.size() == 0 || cities.size() == 0) {
    return City("Chicago", "USA", 3000000);
  }

  // Question indices (in responses vector):
  // Population(0), Weather(1),  Crime(2), CoL(3), Healthcare(4), Pollution(5)
  NarrowByPopulation(responses[0], cities);
  GenerateParameterData();

  vector<vector<double>> all_weights = CalculateWeights(responses);
  int best_match_index = FindBestMatchIndex(all_weights);
  return narrowed_list_[best_match_index];
}

// For use in http query
string Engine::RemoveSpaces(const string& to_change) {
  string city_name = to_change;
  while (city_name.find(' ') != string::npos) {
    int space_index = city_name.find(' ');
    city_name.replace(space_index, 1, "%20");
  }

  return city_name;
}

void Engine::GenerateParameterData() {
  for (homefinder::City& city : narrowed_list_) {

    string city_name = RemoveSpaces(city.name);
    homefinder::API::AddAPIData(kNumbeoUrl + city_name, city);
  }
}

void Engine::NarrowByPopulation(double population,
                                const vector<homefinder::City>& cities) {
  vector<homefinder::City> narrowed_list;
  double error = .1;

  if (population < 100) {
    population = 100; //avoid divide by zero error later
  }

  while (narrowed_list.size() < 10) {
    narrowed_list.clear();

    for (const homefinder::City& city : cities) {
      if (population / city.population <= (1 + error) &&
          population / city.population >= (1 - error)) {
        narrowed_list.push_back(city);
      }
    }

    //if not enough are close, slowly increase allowable error
    error += .02;

    while (narrowed_list.size() > 25) {
      narrowed_list.pop_back();
    }
  }

  narrowed_list_ = narrowed_list;
}

vector<vector<double>> Engine::CalculateWeights(const vector<double>& responses) {
  vector<vector<double>> all_cities_weights;
  for (homefinder::City& city : narrowed_list_) {
    vector<double> city_weights;

    // Question indices (in responses):
    // Population(0), Weather(1), Crime(2), CoL(3), Healthcare(4), Pollution(5)
    for (int i = 1; i < responses.size(); i++) {
      double parameter_weight;

      switch (i) {
        case 1:
          parameter_weight = responses[i] * city.climate_index;
          break;
        case 2:
          parameter_weight = responses[i] * city.crime_index;
          break;
        case 3:
          parameter_weight = responses[i] * city.col_index;
          break;
        case 4:
          parameter_weight = responses[i] * city.healthcare_index;
          break;
        case 5:
          parameter_weight = responses[i] * city.pollution_index;
          break;
      }

      city_weights.push_back(parameter_weight);
    }

    all_cities_weights.push_back(city_weights);
  }

  return all_cities_weights;
}

int Engine::FindBestMatchIndex(const vector<vector<double>>& all_weights) {
  vector<double> to_compare;
  for (const vector<double>& list : all_weights) {
    double total_weight = 0;
    for (const double& weight : list) {
      total_weight += weight;
    }

    to_compare.push_back(total_weight);
  }

  int best_match_index = 0;
  int highest_weight = 0;
  for (int i = 0; i < to_compare.size(); i++) {
    if (to_compare[i] > highest_weight) {
      best_match_index = i;
      highest_weight = to_compare[i];
    }
  }

  return best_match_index;
}
std::vector<homefinder::City> Engine::ParseJSONFile(const string& path) {

  vector<homefinder::City> cities;
  std::ifstream json_file;
  json population_data;
  try {
    json_file.open(
        cinder::app::getAssetPath(path).c_str());
    population_data = json::parse(json_file);
  } catch (std::exception& exception) {
    return cities;
  }


  for (auto& city : population_data) {
    if (city["population"] == nullptr) {
      break;
    }

    homefinder::City new_city(city["city_ascii"], city["country"],
                              city["population"]);
    cities.push_back(new_city);
  }

  return cities;
}

} //namespace homefinder
