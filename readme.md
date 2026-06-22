![DSC04940.JPG](pathfinder_images/image_1.jpg)A month ago, I found myself holding a GNSS module and wondering:

"Can I build a navigation device where I simply enter destination coordinates, and the device continuously shows the distance and direction to the target while I'm walking?"

That simple question became the starting point of this project.

I quickly assembled a rough prototype using an ESP32-C6, a GNSS receiver, and a small display. After writing some test code, I was surprised to see the system successfully calculating the distance to the destination in real time as I moved.

However, there was one major limitation.

GNSS modules can determine movement direction only when the user is actively moving. If you stop walking, the heading information becomes unreliable because the receiver no longer has enough movement data to calculate the travel direction.

While researching solutions, I discovered that dedicated waypoint navigation devices had been used long before smartphone navigation became common. These devices combined position data with a compass to continuously indicate the direction of a destination.

That inspired me to add a digital compass to the project.

By integrating a BMM350 magnetometer, the navigator could now determine its orientation even while standing still. The result was exactly what I had imagined: a compact handheld device capable of displaying both the distance and bearing to a predefined waypoint in real time.

Even more impressive, the system consistently achieved an accuracy of approximately ±5 meters, which is excellent for a DIY navigation project.

The hardware used in this build includes:

1.  DFRobot FireBeetle 2 ESP32-C6
2.  Gravity GNSS Positioning Module
3.  Gravity BMM350 Triple-Axis Magnetometer
4.  Fermion 1.54" 240×240 IPS TFT Display
5.  Rotary Encoder for user input
6.  Piezo Buzzer for feedback
7.  LiPo Battery for portable operation

To complete the project, everything was packaged inside a compact custom enclosure designed in Fusion 360, transforming the prototype into a practical handheld navigation device.

In this tutorial, I'll show you how I designed, assembled, programmed, and tested this DIY Waypoint Navigator (Pathfinder) so you can build one yourself.

![DSC04925.JPG](pathfinder_images/image_2.jpg)![DSC04927.JPG](pathfinder_images/image_3.jpg)

# **Supplies**

**![DSC04821.JPG](pathfinder_images/image_4.jpg)**

**Electronics**

1.  1× [DFRobot FireBeetle 2 ESP32-C6](https://www.dfrobot.com/product-2771.html)
2.  1× [Gravity GNSS Positioning Module](https://www.dfrobot.com/product-2651.html)
3.  1× [Gravity BMM350 Triple-Axis Magnetometer](https://www.dfrobot.com/product-2874.html)
4.  1× [Fermion 1.54" 240×240 IPS TFT Display](https://www.dfrobot.com/product-2072.html)
5.  1× [Rotary Encoder](https://www.dfrobot.com/product-1611.html)
6.  1× [Piezo Buzzer](https://www.mouser.in/ProductDetail/Same-Sky/CMT-1209-390T?qs=i8QVZAFTkqSHIIHkuQW5Og%3D%3D)
7.  1× [Li-Po battery](https://www.mouser.in/ProductDetail/Mikroe/MIKROE-1120?qs=yR1Mpqbr%2FWJ2qp8R3RI4GA%3D%3D)
8.  1× [Mini power switch](https://www.mouser.com/ProductDetail/Adafruit/3870?qs=qSfuJ%252Bfl%2Fd7MMK5fNS31Ow%3D%3D)

**Hardware**

1.  M2 Screw Kit
2.  M3 Screw Kit
3.  Screwdriver

**Fabrication**

1.  Access to a 3D Printer **or**
2.  Pre-printed enclosure parts

**Software**

1.  [Arduino IDE](https://www.arduino.cc/)
2.  [Autodesk Fusion 360](https://www.autodesk.com/in/products/fusion-360/overview) (for enclosure design and modification)

![DSC04824.JPG](pathfinder_images/image_5.jpg)![DSC04826.JPG](pathfinder_images/image_6.jpg)![DSC04827.JPG](pathfinder_images/image_7.jpg)![DSC04834.JPG](pathfinder_images/image_8.jpg)![DSC04835.JPG](pathfinder_images/image_9.jpg)![DSC04830.JPG](pathfinder_images/image_10.jpg)![DSC04833.JPG](pathfinder_images/image_11.jpg)![DSC04854.JPG](pathfinder_images/image_12.jpg)

# **Step 1: What Is GNSS, How Does It Work?**

**![GNSS.png](pathfinder_images/image_13.png)**

Before building the Pathfinder, it's helpful to understand the technology that makes it possible.

**What Is GNSS?**

GNSS stands for **Global Navigation Satellite System**. It is the general term used for satellite-based positioning systems that allow receivers on Earth to determine their location.

Several GNSS constellations are currently operating around the world:

1.  **GPS** (United States)
2.  **GLONASS** (Russia)
3.  **Galileo** (European Union)
4.  **BeiDou** (China)

Modern GNSS receivers can communicate with multiple satellite systems simultaneously, improving accuracy and reducing the time required to obtain a position fix.

**How Does GNSS Work?**

Each GNSS satellite continuously broadcasts signals containing:

1.  Its precise position in space
2.  The exact transmission time
3.  System timing information

The GNSS receiver measures how long these signals take to reach it from multiple satellites.

Since radio signals travel at the speed of light, the receiver can calculate its distance from each satellite.

By combining measurements from at least four satellites, the receiver performs a process called **trilateration** to determine:

1.  Latitude
2.  Longitude
3.  Altitude
4.  Time

The more satellites visible to the receiver, the better the positioning accuracy tends to be.

Simplified Example

Imagine standing somewhere on Earth:

1.  One satellite tells you that you are somewhere on a large sphere.
2.  A second satellite narrows the possible location.
3.  A third satellite reduces the possibilities even further.
4.  A fourth satellite provides enough information to calculate an accurate 3D position.

This process happens continuously, often multiple times per second.

**GNSS in This Project**

The Pathfinder uses a [**DFRobot Gravity GNSS Positioning Module**](https://www.dfrobot.com/product-2651.html) connected to the ESP32-C6.

The GNSS module continuously provides:

1.  Current latitude
2.  Current longitude
3.  Altitude
4.  Ground speed
5.  Number of satellites in view

Once a destination waypoint is entered, the ESP32 compares the current GNSS coordinates with the destination coordinates and calculates:

# **Step 2: Distance to Destination: the Haversine Formula**

**![Distance.png](pathfinder_images/image_14.png)**

Once the navigator knows your current coordinates and the destination coordinates, it needs to calculate the distance between those two points.

At first glance, this sounds simple. However, the Earth is not flat, it's approximately spherical. Because of this, a straight-line calculation using latitude and longitude would produce inaccurate results over longer distances.

To solve this problem, the firmware uses the **Haversine Formula**, a well-known navigation equation that calculates the shortest distance between two points on the surface of a sphere.

The formula takes four inputs:

1.  Current Latitude
2.  Current Longitude
3.  Destination Latitude
4.  Destination Longitude

Using these coordinates, it calculates the **great-circle distance**, which is the shortest path between two points along the Earth's surface.

This distance is continuously updated as the user moves, allowing the navigator to display the remaining distance to the selected waypoint in real time.

In the firmware, the calculated distance is displayed in:

1.  Meters for nearby destinations
2.  Kilometers for longer distances

This provides a simple and intuitive way to track progress toward the target.

# **Step 3: Bearing to Destination**

**![Bearing.png](pathfinder_images/image_15.png)**

Knowing the distance alone isn't enough, you also need to know **which direction to travel**.

This is where the **bearing calculation** comes in.

A bearing is the compass direction from one location to another, measured clockwise from geographic north.

For example:

1.  0° = North
2.  90° = East
3.  180° = South
4.  270° = West

If the navigator calculates a bearing of 90°, the destination is directly east of your current position.

If it calculates a bearing of 225°, the destination lies to the southwest.

The firmware calculates this bearing using the current GNSS coordinates and the destination coordinates.

Example

Imagine you are standing at:

1.  Latitude: 17.4000°
2.  Longitude: 78.5500°

And your destination is:

1.  Latitude: 17.4100°
2.  Longitude: 78.5600°

The bearing calculation determines the direction from your current location to the waypoint. In this example, the destination would be roughly northeast of your position.

# **Step 4: Combining Bearing With the Digital Compass**

**![Compass.png](pathfinder_images/image_16.png)**

The calculated bearing tells us where the destination is relative to **true north**, but it does not tell us which way the device is currently facing.

That's where the BMM350 magnetometer comes in.

The magnetometer continuously measures the device's heading, allowing the ESP32 to determine its current orientation.

The firmware then compares:

**Target Bearing**

−

**Current Heading**

to determine where the destination lies relative to the device.

For example:

1.  Target Bearing = 90° (East)
2.  Device Heading = 45° (North-East)

The destination is 45° to the right of the direction you are currently facing.

The navigation arrow on the display is rotated by this difference, providing real-time guidance toward the selected waypoint.

This combination of **GNSS positioning**, **bearing calculation**, and **digital compass heading** is what allows the Pathfinder to continuously point toward the destination, even when the user is standing still.

# **Step 5: Why I Chose the FireBeetle 2 ESP32-C6**

**![DSC04839.JPG](pathfinder_images/image_17.jpg)**

At the heart of this project is the [**DFRobot FireBeetle 2 ESP32-C6**](https://www.dfrobot.com/product-2771.html)**,** which turned out to be an excellent fit for a portable navigation device.

I chose it for three main reasons:

1.  **Affordable** – At around **$5.90 USD**, it offers excellent value for the features provided.
2.  **Battery Friendly** – It includes a built-in **LiPo battery connector and charging circuit**, eliminating the need for a separate charging module.
3.  **Powerful Enough** – The ESP32-C6 easily handles GNSS data processing, compass calculations, display updates, and user input without being excessive for the task.

Another major advantage is its built-in **Wi-Fi and Bluetooth connectivity**. Instead of entering coordinates directly on the device(Hard Code), I can create a simple web interface that allows waypoint latitude and longitude values to be sent wirelessly over a local network.

This significantly improves the user experience while keeping the hardware simple and compact.

Overall, the FireBeetle 2 ESP32-C6 provided the perfect balance of **cost, features, performance, and power management** for this Pathfinder.

# **Step 6: Designing the Enclosure in Fusion 360**

**![](pathfinder_images/image_18.png)**

I designed a custom enclosure in **Fusion 360** to transform the prototype into a compact handheld device.

The enclosure consists of **three 3D-printed parts**:

**Housing**

The main housing contains mounting points for all major components, including:

1.  FireBeetle 2 ESP32-C6
2.  GNSS Module
3.  BMM350 Magnetometer
4.  Rotary Encoder
5.  Piezo Buzzer
6.  Power Switch

The top section also includes a dedicated mounting area for the GNSS antenna to ensure good satellite reception. An opening is provided on the side for easy access to the ESP32-C6 USB Type-C port for programming and charging.

**Front Cover**

The front cover holds the 1.54" TFT display in place using mounting screws.

It also includes:

1.  Rotary encoder cutout
2.  Decorative graphics on the front panel
3.  Integrated carabiner loop for attaching the navigator to a backpack or gear

**Encoder Knob**

The third part is a custom-designed rotary encoder knob that is 3D printed and press-fits onto the encoder shaft.

All enclosure parts were designed to be easy to print without support material and can be modified to fit different hardware configurations if required.

You can download the Fusion 360 design files and STL files from this project and customize them to suit your own requirements.

[**Pathfinder Fusion 360 File**](https://a360.co/4v1CVzj)

**![IM1.png](pathfinder_images/image_19.png)![Im2.png](pathfinder_images/image_20.png)![IM9.png](pathfinder_images/image_21.png)**

# **Step 7: 3D Printing the Parts**

**![DSC04842.JPG](pathfinder_images/image_22.jpg)**

With the CAD design complete, it's time to print the enclosure.

For this build, I used:

1.  **Orange PLA** for the housing and knob
2.  **Black PLA** for the decorative graphics and Knob

**Parts to Print**

Print the following components:

1.  **1×** [**Housing**](https://content.instructables.com/FRG/IPUD/MPX97XD0/FRGIPUDMPX97XD0.stl?_gl=1*irlhs8*_ga*MTQ1MDcwMjEyMS4xNzc5Nzk0MjUw*_ga_NZSJ72N6RX*czE3ODEyNDI1NzMkbzMxJGcwJHQxNzgxMjQyNTczJGo2MCRsMCRoMA..)
2.  **1×** [**Front Cover**](https://content.instructables.com/FR0/U77L/MPX97XF0/FR0U77LMPX97XF0.stl?_gl=1*irlhs8*_ga*MTQ1MDcwMjEyMS4xNzc5Nzk0MjUw*_ga_NZSJ72N6RX*czE3ODEyNDI1NzMkbzMxJGcwJHQxNzgxMjQyNTczJGo2MCRsMCRoMA..)
3.  **1×** [**Encoder Knob**](https://content.instructables.com/FC5/V4RR/MPX97XH3/FC5V4RRMPX97XH3.stl?_gl=1*irlhs8*_ga*MTQ1MDcwMjEyMS4xNzc5Nzk0MjUw*_ga_NZSJ72N6RX*czE3ODEyNDI1NzMkbzMxJGcwJHQxNzgxMjQyNTczJGo2MCRsMCRoMA..)

I printed all parts using standard PLA settings with a 0.4 mm nozzle and 0.2 mm layer height. No special print settings are required, and all parts can be printed without support material.

![DSC04846.JPG](pathfinder_images/image_23.jpg)![DSC04844.JPG](pathfinder_images/image_24.jpg)![DSC04849.JPG](pathfinder_images/image_25.jpg)![DSC04848.JPG](pathfinder_images/image_26.jpg)

# **Step 8: Mounting the Modules**

With the enclosure printed, it's time to install the electronics into the housing.

**Mounting the ESP32-C6:**

**![DSC04861.JPG](pathfinder_images/image_27.jpg)![](pathfinder_images/image_28.png)**

Start by placing the **FireBeetle 2 ESP32-C6** into its designated mounting position inside the housing.

Make sure the **USB Type-C connector** is properly aligned with the opening in the side of the enclosure.

Once aligned, secure the board using:

1.  **2× M2 screws**

**![ESP_optimized.gif](pathfinder_images/ESP_optimized.gif)**

**Mounting the GNSS and Magnetometer Modules:**

**![DSC04863.JPG](pathfinder_images/image_29.jpg)![](pathfinder_images/image_30.png)**

Next, position the **GNSS Positioning Module** and the **BMM350 Magnetometer Module** on their respective mounting locations.

Align the mounting holes on each module with the standoffs in the housing and secure them using:

1.  **2× M3 screws for the GNSS module**
2.  **2× M3 screws for the BMM350 module**

**![ESP_optimized.gif](pathfinder_images/ESP_optimized.gif)**

Make sure both modules sit flat against the mounting surface before tightening the screws.

![DSC04865.JPG](pathfinder_images/image_31.jpg)![DSC04864.JPG](pathfinder_images/image_32.jpg)

# **Step 9: Mounting the Encoder, Buzzer, and Power Switch**

**![Modules_optimized.gif](pathfinder_images/Modules_optimized.gif")**

With the main modules installed, it's time to mount the user interface components.

**Mounting the Rotary Encoder**

**![](pathfinder_images/image_34.png)**

Place the **rotary encoder** onto the two standoffs provided inside the housing.

Align the mounting holes and secure the encoder using:

1.  **2× M2 screws**

**![Encoder_optimized.gif](pathfinder_images/Encoder_optimized.gif)**

Ensure the encoder shaft is centered in the front panel opening and rotates freely.

**Mounting the Buzzer**

1.  Take the **piezo buzzer** and gently press it into the dedicated slot designed in the housing.
2.  The buzzer should fit snugly in place without requiring additional hardware.

**Mounting the Power Switch**

1.  Next, insert the **power switch** into the switch opening on the housing.
2.  Press it firmly into position until it sits securely in the slot.
3.  If the switch feels loose, apply a **small amount of quick-setting glue** to secure it. Be careful not to get glue inside the switch mechanism, as this may affect its operation.

**![Switch_optimized.gif](pathfinder_images/Switch_optimized.gif)**
![](pathfinder_images/image_35.png)![DSC04868.JPG](pathfinder_images/image_36.jpg)

# **Step 10: Mounting the GNSS Antenna**

Place the **GNSS antenna** into the dedicated antenna compartment in the housing, as shown in the images. The enclosure is designed to hold the antenna securely while keeping it positioned at the top of the device.

Once the antenna is in place, connect the antenna cable to the **GNSS module's antenna connector**. Press the connector straight down until it clicks into place.

Be careful while attaching the connector, as it is small and delicate. Avoid pulling on the cable itself; always handle it by the connector body.

![DSC04871.JPG](pathfinder_images/image_37.jpg)![DSC04872.JPG](pathfinder_images/image_38.jpg)![DSC04874.JPG](pathfinder_images/image_39.jpg)![DSC04873.JPG](pathfinder_images/image_40.jpg)

# **Step 11: Wiring Everything Together**

**![](pathfinder_images/image_41.png)**

Up to this point, the build has been fairly clean and straightforward. Now comes the part that ties everything together: **wiring and soldering**.

**GNSS Module and BMM350 Magnetometer**

Both the GNSS receiver and the BMM350 magnetometer communicate with the ESP32-C6 using the **I²C interface**.

Connect the following pins:

**Module to ESP32-C6**

1.  GND~ GND
2.  VCC~ 3.3V
3.  SDA~ SDA
4.  SCL~ SCL

Since both modules use I²C, they can share the same SDA and SCL lines.

**Rotary Encoder**

Connect the rotary encoder as follows:

**Encoder to ESP32-C6**

GND~ GND

VCC~3.3V

A~ GPIO 6

B~ GPIO 7

C~ GPIO 2

**Buzzer**

Connect the piezo buzzer as follows:

Buzzer to ESP32-C6

GND~ GND

Signal (+) ~ GPIO 4

**Battery and Power Switch**

Connect the LiPo battery to the battery connector on the ESP32-C6.

To allow the device to be switched on and off, place the power switch **in series with the battery's positive wire**.

Wiring Order:

**Battery (+) → Switch → ESP32 Battery (+)**

**Battery (-) → ESP32 Battery (-)**

This allows the switch to disconnect power from the entire system when the device is not in use.

After all connections are complete:

1.  Check all solder joints.
2.  Verify there are no shorts between adjacent wires.
3.  Confirm the battery polarity is correct before powering the device.
4.  Route wires neatly to prevent interference with the enclosure during final assembly.

Once everything is wired and tested, you're ready to install the display and close the enclosure.

![DSC04875.JPG](pathfinder_images/image_42.jpg)

# **Step 12: Mounting the Display**

**![DSC04876.JPG](pathfinder_images/image_43.jpg)**

With all the internal components wired, it's time to install the display into the front cover.

Take the **1.54" TFT display** and position it behind the display opening in the front cover. Align the display's mounting holes with the mounting posts provided in the cover.

Once aligned, secure the display using:

1.  **2× M2 screws**

Make sure the display sits flat against the cover and is centered within the display window for the best appearance.

After mounting, the front cover assembly is ready to be connected to the main housing and prepared for final assembly.

![DSC04877.JPG](pathfinder_images/image_44.jpg)

# **Step 13: Connecting the Display to the ESP32**

**![Display Connection.png](pathfinder_images/image_45.png)**

This is one of the easiest steps in the entire build thanks to the **GDI (General Display Interface) connector** used by the display and the FireBeetle ESP32-C6.

Simply take the **GDI cable** that came with the display and connect one end to the display and the other end to the GDI port on the ESP32-C6.

Since both power and communication signals are carried through this single cable, no additional wiring is required.

Before connecting the cable, make sure to check the **connector orientation** carefully and compare it with the reference image. Connecting the cable incorrectly may prevent the display from working properly.

![DSC04879.JPG](pathfinder_images/image_46.jpg)![DSC04878.JPG](pathfinder_images/image_47.jpg)![DSC04883.JPG](pathfinder_images/image_48.jpg)![DSC04880.JPG](pathfinder_images/image_49.jpg)

# **Step 14: Final Assembly**

**![DSC04884.JPG](pathfinder_images/image_50.jpg)**

With all the electronics installed and tested, it's time to close the enclosure and complete the build.

Carefully place the **LiPo battery** into its the housing. Arrange the wires neatly to prevent them from being pinched when the enclosure is closed.

Next, position the front cover onto the housing and check that:

1.  No wires are trapped between the two parts.
2.  The display cable is routed safely.
3.  The encoder shaft passes cleanly through the cover opening.

![DSC04885.JPG](pathfinder_images/image_51.jpg)![DSC04886.JPG](pathfinder_images/image_52.jpg)

Once everything is aligned, secure the cover using:

1.  **3× M3 screws**

Insert the screws from the back of the cover and tighten them evenly until the enclosure is firmly closed.

![DSC04887.JPG](pathfinder_images/image_53.jpg)![DSC04890.JPG](pathfinder_images/image_54.jpg)

After assembly, install the 3D-printed encoder knob onto the encoder shaft and verify that it rotates smoothly.

![DSC04891.JPG](pathfinder_images/image_55.jpg)![DSC04892.JPG](pathfinder_images/image_56.jpg)![DSC04894.JPG](pathfinder_images/image_57.jpg)

Your DIY Pathfinder is now fully assembled and ready for power-up, configuration, and field testing.

# **Step 15: Flashing the Firmware**

With the hardware assembled, the final step is to upload the Pathfinder firmware to the ESP32-C6.

**Download the Pathfinder Firmware:**

Before uploading the code, download the latest Pathfinder firmware from GitHub: [Pathfinder GitHub Repository](https://github.com/MukeshSankhla/Pathfinder/tree/main?utm_source=chatgpt.com)

1.  Download the zip.
2.  Extract the ZIP file.
3.  Open the **Pathfinder.ino** project in Arduino IDE.

![](pathfinder_images/image_58.png)

**Install Arduino IDE:** If you don't already have it installed, download and install the Arduino IDE from the official website:

*   Download Arduino IDE
*   Install it using the default settings

**Install the ESP32 Board Package:**

*   Open **Arduino IDE**
*   Go to **File → Preferences**
*   In **Additional Boards Manager URLs**, add:

https://espressif.github.io/arduino-esp32/package\_esp32\_index.json

*   Click **OK**
*   Go to **Tools → Board → Boards Manager**
*   Search for **ESP32**
*   Install **ESP32 by Espressif Systems**

Install Required Libraries

**DFRobot GNSS Library:**

**![](pathfinder_images/image_59.png)**

*   Open **Library Manager**
*   Sketch → Include Library → Manage Libraries
*   Search for:

DFRobot\_GNSS

*   Click **Install**

**DFRobot GDL Display Library:**

**![](pathfinder_images/image_60.png)**

Search for:

DFRobot\_GDL

and install the latest version.

**Install the BMM350 Library:**

The BMM350 library must currently be installed manually.

*   Visit the DFRobot BMM350 GitHub repository: [DFRobot BMM350 GitHub Repository](https://github.com/DFRobot/DFRobot_BMM350?utm_source=chatgpt.com)
*   Click **Code → Download ZIP**
*   In Arduino IDE go to: **Sketch → Include Library → Add .ZIP Library**
*   Select the downloaded ZIP file.

The library will now be added to your Arduino installation.

**Select the Correct Board:**

**![](pathfinder_images/image_61.png)**

Connect the FireBeetle 2 ESP32-C6 to your computer using a USB Type-C cable.

Then configure Arduino IDE:

*   **Board:** DFRobot FireBeetle 2 ESP32-C6
*   **Port:** Select the COM port corresponding to your device

*   Click **Upload**.
*   Wait for compilation and flashing to complete.

![](pathfinder_images/image_62.png)

Once the upload finishes successfully, the Pathfinder will reboot automatically and display the startup screen.

**![Boot_optimized.gif](pathfinder_images/Boot_optimized.gif)**

# **Step 16: Connect to the Access Point and Configure Pathfinder**

With the firmware flashed, it's time to configure your Pathfinder.

**Connect to the Pathfinder Wi-Fi Network:**

Power on the Pathfinder using the power switch.

After booting, the device will create its own Wi-Fi access point.

1.  Open the Wi-Fi settings on your phone or computer.
2.  Look for the Pathfinder hotspot.
3.  Connect using the password:

X123456789

**Open the Configuration Portal:**

Once connected to the Pathfinder network, open a web browser and navigate to:

192.168.0.4

You should see the Pathfinder configuration interface.

![](pathfinder_images/image_63.png)

**Add Waypoints:**

Using the web interface, enter the details for each location you want to save:

1.  Location Name
2.  Latitude
3.  Longitude

Click **Save** to store the waypoint.

You can add multiple locations that will later be available directly on the Pathfinder.

**Set Your Time Zone:**

The web interface also provides a time zone selection menu.

Simply:

1.  Select your local time zone.
2.  Click **Save**.

This ensures the device displays the correct local time.

**Persistent Storage:**

All waypoint and time zone information is stored in the ESP32's **Non-Volatile Storage (NVS)**.

This means your settings remain saved even after the device is powered off.

Once saved, the locations are automatically loaded and become available within the Pathfinder menu system.

**Calibrate the Compass:**

**![Calibaration_optimized.gif](pathfinder_images/Calibaration_optimized.gif)**

Before using Pathfinder for navigation, the compass must be calibrated.

Start Calibration

1.  Double-click the encoder knob to open the menu.
2.  Scroll using the rotary encoder.
3.  Select **Settings**.
4.  Open **Calibrate Compass**.

The calibration process will guide you through eight compass directions:

1.  North (N)
2.  North-East (NE)
3.  East (E)
4.  South-East (SE)
5.  South (S)
6.  South-West (SW)
7.  West (W)
8.  North-West (NW)

For each direction:

1.  Physically face the indicated direction.
2.  Press the encoder knob to record the measurement.
3.  Move to the next direction and repeat.

This is typically a **one-time setup** unless the device is moved to a new environment or the compass performance changes.

**Troubleshooting:**

If you notice that the navigation arrow is not pointing accurately toward the destination:

1.  Repeat the compass calibration process.
2.  Ensure you are facing the correct direction during each calibration step.
3.  Keep the device away from large metal objects or strong magnetic fields while calibrating.

**Start Navigating:**

**![SelectLoc_optimized.gif](pathfinder_images/SelectLoc_optimized.gif)**

Once your locations have been added and the compass has been calibrated:

1.  Double-click the encoder knob to open the menu.
2.  Select a saved location.

The main screen will now display:

1.  Distance to the selected destination
2.  Direction arrow pointing toward the target
3.  Additional navigation information

As you move, the distance updates in real time while the compass continuously keeps the navigation arrow pointed toward your selected waypoint.

# **Step 17: How the Pathfinder Code Works**

**![ChatGPT Image Jun 12, 2026, 01\_44\_23 PM.png](pathfinder_images/image_64.png)**

The Pathfinder firmware combines data from the GNSS receiver and the BMM350 magnetometer to create a real-time waypoint navigation system.

At a high level, the firmware performs five main tasks:

**1\. Read GNSS Data**

The GNSS module continuously provides:

1.  Latitude
2.  Longitude
3.  Altitude
4.  Speed
5.  Satellite count
6.  UTC Time

This information is used to determine the device's current position anywhere on Earth.

**2\. Load Saved Waypoints**

When the device starts, it loads all saved waypoints and settings from the ESP32's **Non-Volatile Storage (NVS)**.

These include:

1.  Saved locations
2.  Latitude and longitude values
3.  Selected time zone
4.  Device settings

Because the data is stored in NVS, it remains available even after the device is powered off.

**3\. Calculate Distance and Bearing**

Once a waypoint is selected, the firmware compares:

1.  Current GNSS coordinates
2.  Destination coordinates

Using the **Haversine Formula**, it calculates the shortest distance between the two points on the Earth's surface.

The code also calculates the **bearing**, which represents the direction of the destination relative to true north.

The result is:

1.  Distance to target
2.  Direction of target

**4\. Read the Magnetometer**

The BMM350 continuously measures the Earth's magnetic field.

Using these readings, the firmware calculates the device's current heading.

Unlike GNSS heading, the compass continues to work even when the user is standing still.

This allows Pathfinder to always know which direction it is facing.

**5\. Draw the Navigation Arrow**

The final navigation arrow is calculated using:

Arrow Angle = Target Bearing - Current Heading

If the destination is directly ahead, the arrow points straight up.

If the destination is to the left or right, the arrow rotates accordingly.

This process repeats continuously, providing real-time navigation guidance.

Debugging Compass Direction Issues

Depending on how the magnetometer is mounted inside the enclosure, the compass direction may not match the expected orientation.

**Common symptoms include:**

1.  Arrow points in the opposite direction
2.  Arrow appears mirrored
3.  North and South are swapped
4.  East and West are reversed

To correct this, Pathfinder includes two compass configuration settings:

// ==========================================

// COMPASS ORIENTATION SETTINGS

// ==========================================

int heading\_offset = 180;

bool reverse\_compass = true;

**heading\_offset:** This value rotates the entire compass heading.

Example:

int heading\_offset = 0;

No rotation applied.

int heading\_offset = 90;

Compass rotates by 90 degrees.

int heading\_offset = 180;

Compass rotates by 180 degrees.

If your compass consistently points in the wrong direction by a fixed amount, adjust this value.

Typical values:

1.  0°
2.  90°
3.  180°
4.  270°

**reverse\_compass:** This setting reverses the compass rotation direction.

bool reverse\_compass = false;

Normal operation.

bool reverse\_compass = true;

Reverse heading calculation.

Use this if:

1.  Turning left causes the arrow to move right.
2.  Turning right causes the arrow to move left.
3.  Compass behavior appears mirrored.

Quick Debug Procedure

After uploading the firmware:

Test 1: Face North

Use your phone's compass and face north.

The Pathfinder heading should display approximately: 0° or 360°

Test 2: Rotate Clockwise

Turn slowly clockwise.

The heading should increase smoothly:

0° → 90° → 180° → 270°

If the values decrease instead, try:

reverse\_compass = !reverse\_compass;

Test 3: Check Orientation

If North appears as South:

heading\_offset = 180;

If North appears as East:

heading\_offset = 90;

If North appears as West:

heading\_offset = 270;

These two settings allow the firmware to compensate for different magnetometer mounting orientations without requiring any hardware changes.

# **Conclusion**

**![DSC04931.JPG](pathfinder_images/image_65.jpg)![](pathfinder_images/image_66.png)**

Building Pathfinder was a fascinating journey that combined embedded electronics, GNSS positioning, digital compasses, wireless configuration, firmware development, 3D design, and product prototyping into a single project.

What started as a simple question "Can I build a device that continuously shows the distance and direction to a destination?" eventually became a fully functional handheld waypoint navigator capable of guiding users to predefined locations in real time.

What fascinated me most about this project was discovering that the concept itself is not new. While building it, I learned that waypoint navigators and similar navigation aids existed decades before smartphones and Google Maps became part of our daily lives.

It made me appreciate the engineers and inventors of that era. They achieved incredible things with limited tools, limited computing power, and without the software ecosystems, development frameworks, AI assistants, and resources that we have today.

With modern hardware, open-source libraries, and a bit of coding, I was able to recreate a similar system in just a couple of days. That realization reminded me that innovation doesn't always mean inventing something completely new. Sometimes the best ideas come from revisiting old concepts and rebuilding them with modern technology.

Along the way, I learned more about GNSS systems, navigation mathematics, magnetometer calibration, embedded user interfaces, and the practical challenges of turning a prototype into a portable device.

My biggest takeaway from this project is simple:

**Think like the pioneers of the past, but build with the tools of today.**

I hope this project inspires you to explore navigation technology, learn something new, and perhaps build your own version of Pathfinder. If you do, I'd love to see what improvements and ideas you add to it.

Happy building!