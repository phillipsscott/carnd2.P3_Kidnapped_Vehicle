/*
* particle_filter.cpp
*
*  Created on: Dec 12, 2016
*      Author: Tiffany Huang
*/

#include <random>
#include <algorithm>
#include <iostream>
#include <numeric>
#include <math.h>
#include <iostream>
#include <sstream>
#include <string>
#include <iterator>

#include "particle_filter.h"

using namespace std;

#define EPS 0.00001

void ParticleFilter::init(double x, double y, double theta, double std[]) {
	if (is_initialized) {
		return;
	}

	num_particles = 100;

	normal_distribution<double> dist_x(x, std[0]);
	normal_distribution<double> dist_y(y, std[1]);
	normal_distribution<double> dist_theta(theta, std[2]);
	default_random_engine gen;

	for (int i = 0; i < num_particles; i++) {
		Particle particle;
		particle.id = i;
		particle.x = dist_x(gen);
		particle.y = dist_y(gen);
		particle.theta = dist_theta(gen);
		particle.weight = 1.0;

		particles.push_back(particle);
	}

	is_initialized = true;
}

void ParticleFilter::prediction(double delta_t, double std_pos[], double velocity, double yaw_rate) {
	normal_distribution<double> dist_x(0, std_pos[0]);
	normal_distribution<double> dist_y(0, std_pos[1]);
	normal_distribution<double> dist_theta(0, std_pos[2]);

	for (auto& p : particles) {
		if (fabs(yaw_rate) < 0.00001) {  
			p.x += velocity * delta_t * cos(p.theta);
			p.y += velocity * delta_t * sin(p.theta);

		}
		else {
			p.x += velocity / yaw_rate * (sin(p.theta + yaw_rate * delta_t) - sin(p.theta));
			p.y += velocity / yaw_rate * (cos(p.theta) - cos(p.theta + yaw_rate * delta_t));
			p.theta += yaw_rate * delta_t;
		}

		p.x += dist_x(gen);
		p.y += dist_y(gen);
		p.theta += dist_theta(gen);
	}

}

void ParticleFilter::dataAssociation(std::vector<LandmarkObs> predicted, std::vector<LandmarkObs>& observations) {
	
	for (auto& obs : observations) {
		double minD = numeric_limits<float>::max();

		for (const auto& pred : predicted) {
			double distance = dist(obs.x, obs.y, pred.x, pred.y);
			if (minD > distance) {
				minD = distance;
				obs.id = pred.id;
			}
		}
	}
}



void ParticleFilter::updateWeights(double sensor_range, double std_landmark[],
	std::vector<LandmarkObs> observations, Map map_landmarks) {
	
	double stdLandmarkRange = std_landmark[0];
	double stdLandmarkBearing = std_landmark[1];

	for (auto& p : particles) {
		double x = p.x;
		double y = p.y;
		double theta = p.theta;

		double sensor_range_2 = sensor_range * sensor_range;
		vector<LandmarkObs> predictions;

		for (const auto& lm : map_landmarks.landmark_list) {
			float landmarkX = lm.x_f;
			float landmarkY = lm.y_f;
			int id = lm.id_i;
			double dX = p.x - landmarkX;
			double dY = p.y - landmarkY;
			if (dX*dX + dY * dY <= sensor_range_2) {
				predictions.push_back(LandmarkObs{ id, landmarkX, landmarkY });
			}
		}

		vector<LandmarkObs> mappedObservations;
		for (const auto& obs : observations) {
			double xx = cos(theta)*obs.x - sin(theta)*obs.y + x;
			double yy = sin(theta)*obs.x + cos(theta)*obs.y + y;
			mappedObservations.push_back(LandmarkObs{ obs.id, xx, yy });
		}

		dataAssociation(predictions, mappedObservations);

		p.weight = 1.0;
		for (const auto& obs_m : mappedObservations) {
			double observationX = obs_m.x;
			double observationY = obs_m.y;
			int landmarkId = obs_m.id;

			double landmarkX, landmarkY;
			unsigned int k = 0;
			unsigned int nLandmarks = predictions.size();
			bool found = false;
			while (!found && k < nLandmarks) {
				if (predictions[k].id == landmarkId) {
					found = true;
					landmarkX = predictions[k].x;
					landmarkY = predictions[k].y;
				}
				k++;
			}

			double dX = observationX - landmarkX;
			double dY = observationY - landmarkY;

			double weight = (1 / (2 * M_PI*stdLandmarkRange*stdLandmarkBearing)) * exp(-(dX*dX / (2 * stdLandmarkRange*stdLandmarkRange) + (dY*dY / (2 * stdLandmarkBearing*stdLandmarkBearing))));
			if (weight == 0) {
				p.weight *= EPS;
			}
			else {
				p.weight *= weight;
			}
		}
	}
}

void ParticleFilter::resample() {
	vector<double> weights;
	double maxWeight = numeric_limits<double>::min();
	for (int i = 0; i < num_particles; i++) {
		weights.push_back(particles[i].weight);
		if (particles[i].weight > maxWeight) {
			maxWeight = particles[i].weight;
		}
	}

	uniform_real_distribution<double> distDouble(0.0, maxWeight);
	uniform_int_distribution<int> distInt(0, num_particles - 1);

	int index = distInt(gen);

	double beta = 0.0;

	vector<Particle> resampledParticles;
	for (int i = 0; i < num_particles; i++) {
		beta += distDouble(gen) * 2.0;
		while (beta > weights[index]) {
			beta -= weights[index];
			index = (index + 1) % num_particles;
		}
		resampledParticles.push_back(particles[index]);
	}

	particles = resampledParticles;
}

Particle ParticleFilter::SetAssociations(Particle particle, std::vector<int> associations, std::vector<double> sense_x, std::vector<double> sense_y)
{
	particle.associations.clear();
	particle.sense_x.clear();
	particle.sense_y.clear();

	particle.associations = associations;
	particle.sense_x = sense_x;
	particle.sense_y = sense_y;

	return particle;
}

string ParticleFilter::getAssociations(Particle best)
{
	vector<int> v = best.associations;
	stringstream ss;
	copy(v.begin(), v.end(), ostream_iterator<int>(ss, " "));
	string s = ss.str();
	s = s.substr(0, s.length() - 1);  
	return s;
}
string ParticleFilter::getSenseX(Particle best)
{
	vector<double> v = best.sense_x;
	stringstream ss;
	copy(v.begin(), v.end(), ostream_iterator<float>(ss, " "));
	string s = ss.str();
	s = s.substr(0, s.length() - 1);  
	return s;
}
string ParticleFilter::getSenseY(Particle best)
{
	vector<double> v = best.sense_y;
	stringstream ss;
	copy(v.begin(), v.end(), ostream_iterator<float>(ss, " "));
	string s = ss.str();
	s = s.substr(0, s.length() - 1);  
	return s;
}
