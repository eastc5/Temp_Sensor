## Temperature Sensor and Heathrow data 

This is a simple project which combines data from an Arduino temperature sensor and metoffice temperature observations at Heathrow airport and graphs them in a simple R plot.

The Arduino code is a thingspeak webclient finite state machine which i adapted from Arduinomstr's original pachube code back in 2012, I couldn't find it when i googled it again so wanted it up here to preserve it as it is extremely useful. The ardunio temperature sensor has run successfully with only minor interruptions for 4 years now and the data has been stored to thingspeak which has created a fantastic dataset.

The R code is a good example of parsing JSON into R from two different api data sources and creating a simple graph. My thingspeak data stream is private, as its my living room temperature and its fairly easy to tell when someone is at home from it. Heathrow is the closest metoffice observation point available to me. If you want to use the code you will need to get your own api keys.

The Arduino setup is a Uno board with Ethernet shield and a one wire digital temperature sensor DS18B20 