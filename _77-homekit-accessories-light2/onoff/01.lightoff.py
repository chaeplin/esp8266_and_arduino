#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import paho.mqtt.publish as publish

publish.single("light/bedroomlight", "Off", hostname="127.0.0.1")
publish.single("light/livingroomlight", "Off", hostname="127.0.0.1")