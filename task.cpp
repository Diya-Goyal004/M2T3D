#include <iostream>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <algorithm>
#include <ctime>
#include <chrono>

using namespace std;
using namespace std::chrono;

// Structure to represent traffic signal data
struct TrafficSignalData {
    time_t timestamp;
    int light_id;
    int num_cars;

    TrafficSignalData(time_t ts, int id, int cars) : timestamp(ts), light_id(id), num_cars(cars) {}
};

// Bounded-buffer queue implementation
class BoundedBufferQueue {
private:
    queue<TrafficSignalData> buffer;
    mutex mtx;
    condition_variable not_full;
    condition_variable not_empty;
    const size_t max_size;

public:
    BoundedBufferQueue(size_t size) : max_size(size) {}

    void add(const TrafficSignalData& data) {
        unique_lock<mutex> lock(mtx);
        not_full.wait(lock, [this]() { return buffer.size() < max_size; });
        buffer.push(data);
        not_empty.notify_one();
    }

    TrafficSignalData remove() {
        unique_lock<mutex> lock(mtx);
        not_empty.wait(lock, [this]() { return !buffer.empty(); });
        TrafficSignalData data = buffer.front();
        buffer.pop();
        not_full.notify_one();
        return data;
    }
};

// Function to generate random traffic signal data
TrafficSignalData generateTrafficData(int light_id) {
    time_t now = time(nullptr);
    int num_cars = rand() % 100; // Random number of cars passed
    return TrafficSignalData(now, light_id, num_cars);
}

// Function to simulate traffic producers
void producerFunction(int id, BoundedBufferQueue& buffer, int num_measurements_per_hour) {
    const int num_iterations = 12; // Number of measurements per hour
    for (int i = 0; i < num_iterations; ++i) {
        TrafficSignalData data = generateTrafficData(id);
        buffer.add(data);
        cout << "Producer " << id << " added data: Light ID = " << data.light_id << ", Cars Passed = " << data.num_cars << endl;
        this_thread::sleep_for(minutes(5)); // Measurement every 5 minutes
    }
}

// Function to simulate traffic consumers
void consumerFunction(BoundedBufferQueue& buffer, int num_traffic_lights, int top_n) {
    vector<int> trafficCount(num_traffic_lights, 0);
    time_t last_hour = time(nullptr);

    while (true) {
        TrafficSignalData data = buffer.remove();
        trafficCount[data.light_id] += data.num_cars;

        time_t current_time = time(nullptr);
        // Check if an hour has passed
        if (current_time - last_hour >= 3600) {
            vector<pair<int, int>> trafficLights; // <light_id, num_cars>
            for (int i = 0; i < num_traffic_lights; ++i) {
                trafficLights.push_back({i, trafficCount[i]});
            }

            // Sort traffic lights based on the number of cars passed
            sort(trafficLights.begin(), trafficLights.end(), [](const pair<int, int>& a, const pair<int, int>& b) {
                return a.second > b.second;
            });

            // Print the top N most congested traffic lights
            cout << "Top " << top_n << " congested traffic lights:\n";
            for (int i = 0; i < top_n && i < trafficLights.size(); ++i) {
                cout << "Light ID: " << trafficLights[i].first << ", Cars Passed: " << trafficLights[i].second << endl;
            }

            // Reset traffic count for the next hour
            trafficCount.assign(num_traffic_lights, 0);
            last_hour = current_time; // Update last hour
        }
    }
}

int main() {
    const int num_traffic_lights = 5; // Number of traffic lights
    const int num_producers = 3; // Number of producer threads
    const int num_consumers = 1; // Number of consumer threads
    const int top_n = 3; // Top N congested traffic lights

    srand(time(nullptr)); // Seed for random number generation

    BoundedBufferQueue buffer(100); // Bounded buffer queue

    // Create producer threads
    vector<thread> producers;
    for (int i = 0; i < num_producers; ++i) {
        producers.emplace_back(producerFunction, i % num_traffic_lights, ref(buffer), num_traffic_lights);
    }

    // Create consumer threads
    vector<thread> consumers;
    for (int i = 0; i < num_consumers; ++i) {
        consumers.emplace_back(consumerFunction, ref(buffer), num_traffic_lights, top_n);
    }

    // Join producer threads
    for (auto& producer : producers) {
        producer.join();
    }

    // Join consumer threads
    for (auto& consumer : consumers) {
        consumer.join();
    }

    return 0;
}
