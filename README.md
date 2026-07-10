A simple water level sensor that uses the capacitive/touch pins on an esp-32 to measure the water level in a container.
It is preferable if the container walls are less than 0.8cms in thickness and if the shape is regular
the container surface has to be a non conductive material 
it is required to first calibrate the sensor readings with and without water at each level to get accurate readings 
this can be integrated with an web app or the values can be used to make a graphical interface in order to represent the water level

it is required of the user to connect jumper wires to each of the capacitive ports mentioned in the code and then attach the other male end of the wire with a 1cm X 1cm aluminium foil
this helps increase the contact surface, thus giving a better reading.
connect all of these aluminium foil attached wires at the respective required percentages of water in the bottle and insulate them with duct tape so they do not touch each other

Congratulations! Now you can run the calibration code on the esp-32, open the serial monitor, calibrate the values.
once you get the calibrated values, note them down and input those values in the main sensing/filtering code and start using the program.
You can tap into the web server by connecting to the esp-32 wifi. it has a visual representation of the container with a live water level reading!
