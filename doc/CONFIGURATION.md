Configuration file reference
============================

The configuration file is formatted in XML and subdivided in 4 sections:

* Default options, shared among every device, user and service
* Devices declaration and settings
* Users declaration and settings
* Services declaration and settings

The syntax is the following:

```xml
<configuration>
    <defaults>
        <!-- default options -->
    </defaults>

    <devices>
        <!-- devices definitions -->
    </devices>

    <users>
        <!-- users definitions -->
    </users>

    <services>
        <!-- services definitions -->
    </services>
</configuration>
```

Options
-------

<table>
    <tr>
        <th>Name</th>
        <th>Type</th>
        <th>Default</th>
        <th>Description</th>
    </tr>
    
    <tr>
        <td>enable</td>
        <td>Boolean</td>
        <td>true</td>
        <td>Enable pam_usb</td>
    </tr>

    <tr>
        <td>debug</td>
        <td>Boolean</td>
        <td>false</td>
        <td>Enable debug messages</td>
    </tr>
    
    <tr>
        <td>quiet</td>
        <td>Boolean</td>
        <td>false</td>
        <td>Quiet mode</td>
    </tr>
    
    <tr>
        <td>color_log</td>
        <td>Boolean</td>
        <td>true</td>
        <td>Enable colored output</td>
    </tr>
    
    <tr>
        <td>one_time_pad</td>
        <td>Boolean</td>
        <td>true</td>
        <td>Enable the use of one time pads</td>
    </tr>
    
    <tr>
        <td>deny_remote</td>
        <td>Boolean</td>
        <td>true</td>
        <td>Deny access from remote host (ssh)</td>
    </tr>
    
    <tr>
        <td>probe_timeout</td>
        <td>Time</td>
        <td>10s</td>
        <td>Time to wait for the volume to be detected</td>
    </tr>
    
    <tr>
        <td>pad_expiration</td>
        <td>Time</td>
        <td>1h</td>
        <td>Time between pads regeneration</td>
    </tr>
    
    <tr>
        <td>hostname</td>
        <td>String</td>
        <td>Computer's hostname</td>
        <td>Must be unique accross computers using the same device</td>
    </tr>

    <tr>
        <td>system_pad_directory</td>
        <td>String</td>
        <td>.pamusb</td>
        <td>Relative path to the user's home used to store one time pads</td
    </tr>
    
    <tr>
        <td>device_pad_directory</td>
        <td>String</td>
        <td>.pamusb</td>
        <td>Relative path to the device used to store one time pads</td>
    </tr>
</table>

Example:

```xml
<configuration>
    <defaults>
        <!-- Disable colored output by default -->
        <option name="color_log">false</option>
        <!-- Enable debug output -->
        <option name="debug">true</option>
    </defaults>
 
    <users>
        <user id="root">
            <!-- Enable colored output for user "root" -->
            <option name="color_log">true</option>
        </user>

        <user id="scox">
             <!-- Disable debug output for user "scox" -->
             <option name="debug">false</option>
        </user>
    </users>

    <devices>
        <device id="mydevice">
            <!-- Wait 15 seconds instead of the default 10 seconds for "mydevice" to be detected -->
            <option name="probe_timeout">15</option>
        </device>
    </devices>

    <services>
        <service id="su">
            <!-- Disable pam_usb for "su" ("su" will ask for a password as usual) -->
            <option name="enable">false<option>
       </service>
    </services>
</configuration>
```

Devices
-------

<table>
    <tr>
        <th>Name</th>
        <th>Type</th>
        <th>Description</th>
        <th>Example</th>
    </tr>
    
    <tr>
        <td>id</td>
        <td>Attribute</td>
        <td>Arbitrary device name</td>
        <td>MyDevice</td>
    </tr>
    
    <tr>
        <td>vendor</td>
        <td>Element</td>
        <td>device's vendor name</td>
        <td>SanDisk Corp.</td>
    </tr>
    
    <tr>
        <td>model</td>
        <td>Element</td>
        <td>device's model name</td>
        <td>Cruzer Titanium</td>
    </tr>
    
    <tr>
        <td>serial</td>
        <td>Element</td>
        <td>serial number of the device</td>
        <td>SNDKXXXXXXXXXXXXXXXX</td>
    </tr>
    
    <tr>
        <td>volume_uuid</td>
        <td>Element</td>
        <td>UUID of the device's volume used to store pads</td>
        <td>6F6B-42FC</td>
    </tr>
</table>

Example:

```xml
<device id="MyDevice">
    <vendor>SanDisk Corp.</vendor>
    <model>Cruzer Titanium</model>
    <serial>SNDKXXXXXXXXXXXXXXXX</serial>
    <volume_uuid>6F6B-42FC</volume_uuid>
</device>
```

Users
-----

<table>

    <tr>
        <th>Name</th>
        <th>Type</th>
        <th>Description</th>
        <th>Example</th>
    </tr>
    
    <tr>
        <td>id</td>
        <td>Attribute</td>
        <td>Login of the user</td>
        <td>root</td>
    </tr>
    
    <tr>
        <td>device</td>
        <td>Element</td>
        <td>id of the device associated to the user</td>
        <td>MyDevice</td>
    </tr>
    
    <tr>
        <td>agent</td>
        <td>Element</td>
        <td>Agent commands, for use with pamusb-agent</td>
    </tr>
</table>

Example:

```xml
<user id="scox">
    <device>MyDevice</device>
    
    <!-- When the user "scox" removes the usb device, lock the screen and pause
    beep-media-player -->
    <agent event="lock">gnome-screensaver-command --lock</agent>
    <agent event="lock">beep-media-player --pause</agent>
    
    <!-- Resume operations when the usb device is plugged back and authenticated -->
    <agent event="unlock">gnome-screensaver-command --deactivate</agent>
    <agent event="unlock">beep-media-player --play</agent>
</user>
```

Example for multiple devices:

```xml
<user id="scox">
    <device>MyDevice</device>
    <device>MyDevice2</device>

    <!-- When the user "scox" removes the usb device MyDevice, lock the screen -->
    <agent event="lock" device="MyDevice">gnome-screensaver-command --lock</agent>
    
    <!-- Pause beep-media-player when MyDevice2 is removed for user "scox" -->
    <agent event="lock" device="MyDevice2">beep-media-player --pause</agent>
    
    <!-- Resume operations when any usb device is plugged back and authenticated -->
    <agent event="unlock">gnome-screensaver-command --deactivate</agent>
    <agent event="unlock">beep-media-player --play</agent>
</user>
```

Services
--------

<table>
    <tr>
        <th>Name</th>
        <th>Type</th>
        <th>Description</th>
        <th>Example</th>
    </tr>
    
    <tr>
        <td>id</td>
        <td>Attribute</td>
        <td>Name of the service</td>
        <td>su</td>
    </tr>
</table>

Example:

```xml
<service id="su">
    <!--
       Here you can put service specific options such as "enable", "debug" etc.
       See the options section of this document.
    -->
</service>
```

Location of the configuration file
----------------------------------

By default, pam_usb.so and its tools will look for the configuration file at `/etc/pamusb.conf`. 

If you want to use a different location, you will have to use the `-c` flag.

    # /etc/pam.d/common-auth
    auth    sufficient      pam_usb.so -c /some/other/path.conf
    auth    required        pam_unix.so nullok_secure

You will also have to use the -c option when calling pam_usb's tools.

    pamusb-agent -c /some/other/path.conf
