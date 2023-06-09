# Concept

## Problem

Today irrigation does often not keep into account possible water leaks from watering pipes. This superficial management is leading to a huge waste of water, which is one of main causes of drought. Agriculture is the most water demanding sector, with the 40% of total water usage per year in Europe.
In Italy over 26 billion of cube meters of water per year are consumed: around 55% is reserved for agriculture, 27% for the industry and 18% for the city. However, withdrawal of water effectively exceeds 33 billion of cube meters per year: this difference is motivated by the waste of around 22% of total water and 17% of these leakages are related to the agricultural sector. However, our project can also be used to monitor the state of regular water distribution system, if a proper adaptation is given.

## Proposed solution

Our proposed solution consists of an interface through which it is possible to monitor the irrigation system. The user can know if a leakage is detected and where it is located, as well as the flow of water at the source site. Another available information is the past water usage schedule, with the relative periodical measurements of water flows.

## Use cases

Our system allows to do the following tasks:

* Leakages detection, quantification and localization
* Water usage monitoring

## User requirements

The starting point of our analysis is the collection of user requirements. In our context, there are some fixed parameters, mainly related to the chosen infrastructure, while other parameters should be finetuned according to the specific final user of the application, ideally a farmer owning a variable field. To cope with this issue, we made reasonable assumptions about these parameters, and we also guaranteed the scalability of the system according to user needs.

### Alarm system

We thought about suitable actuators to use in order to indicate an alarm condition. A simple LED with a buzzer are certainly a good option.
Obiouvsly, it makes no sense to place these actuators in the middle of a field, because it is unlikely that someone will see or hear something. Instead, they can be placed near water source, so they can be easily noticed and constantly connected to electric power.

### Events

The main events we are interested in are certainly water flow changes. We wish to monitor the water flow at the source and also to know about current possible leakages as the results of a proper water leakage detection algorithm comparing flow changes at different nodes of the irrigation system.

### Communication protocol

LoRa is a suitable technology for different reasons:

* It uses low bandwidth, and this is a great advantage in our context. In fact, we need to exchange simple data, namely water flow measurements, so LoRa is certainly a good option.
* It can send data on long ranges. It is not an immediate advantage in our architecture, because we implemented a peer-to-peer communication with adjacent nodes. However, it can be very useful firstly to reach also far LoRa gateways and on large scale systems to exchange data between distant nodes, if needed.
* It works with low power consumption, and this is a crucial added value to the protocol since we use battery-powered MCUs.

### Energy consumption

* We thought about the best battery lifetime for our MCUs and our conclusion was to reason over the needed sampling cycles and the utilization period of the system. So, 1 year of battery life was considered as a good option.
* We also thought about the possibility to recover energy from water flow sensor dynamics, for example with a dynamo. However, this solution was not feasible for our hardware because of the impossibility to access to the enclosed rotor in a proper way.

## HOW WE HAVE WORKED
We have created three subtopic, you can see all this work in [Design](https://github.com/simonescaccia/Irrigation-Water-Leakage-System/blob/main/Design.md) and [Evaluation](https://github.com/simonescaccia/Irrigation-Water-Leakage-System/blob/main/Evaluation.md):
1. Prototype:
* build the physical prototype, solving the technical problems
* create different algorithms to synchronize the nodes
* connect prototype and AWS with MQTT
* take data to choose the best algorithm
* take data to set a correct threshold
2. Simulation in IoT-Lab:
* build the infrastructure of the simulation, solving the technical problems
* send data between nodes with lora-semtech
* create a sample generator function to simulate water flow rates and leakages
* take data of the energy consumption
3. Connection of IoT-Lab and AWS:
* create a node that send data to TTN using the sample generator function
* integrate TTN and AWS
* create the rules, the dynamoDB and the lambda functions to work with the data
* create the website page with Amplify

## Possible improvements
There are some possible improvements for the future of this project:
* Fixing the problem of the libraries lora-semtech and lora-wan to remove the redundant Source TTN
* Transpose the code of the simulation in a real environment using some boards with a working lora-wan library
* Adding the buzzer and the led
* Using real samples instead of simulated ones, in IoT-Lab
* Crash of the node when receiving messages with a payload higher than 32 bytes, in IoT-Lab
