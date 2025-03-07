import base64
import datetime
import hashlib
import json
import logging
import os
import pprint
import random
import string
import sys
import time
import threading
from typing import Any, Dict
import uuid


# pip install flask
from flask import Flask, abort, jsonify, make_response, redirect, render_template, request, send_file
from flask_inflate import Inflate       # pip install flask-inflate
from werkzeug.utils import secure_filename
from werkzeug.middleware.proxy_fix import ProxyFix

# The Simpson's Tapped Out protobufs
import AuthData_pb2
import ClientConfigData_pb2
import ClientLog_pb2
import ClientMetrics_pb2
import ClientTelemetry_pb2
import Common_pb2
import CustomerServiceData_pb2
import Error_pb2
import GambleData_pb2
import GameplayConfigData_pb2
import GetFriendData_pb2
import LandData_pb2
import MatchmakingData_pb2
import OffersData_pb2
import PurchaseData_pb2
import WholeLandTokenData_pb2

import auth_manager

tsto_events = {
    0: "Normal Play",
    1348228800: "The first new level : Level 21",
    1348833600: "Episode Tie-in : Season Premiere 2012 Moonshine River",
    1349265600: "Level 22 and Event : Treehouse of Horror XXIII",
    1352116800: "Level 23",
    1352376000: "Event : Thanksgiving 2012",
    1352808000: "Episode Tie-in : Penny-Wiseguys",
    1354276800: "Episode Tie-in : The Day the Earth Stood Cool",
    1354708800: "Level 24 and Event : Christmas 2012",
    1357905600: "Level 25",
    1359547200: "Event : Valentine's Day 2013 and Level 26 Pre-release",
    1360843200: "Level 26",
    1362052800: "Episode Tie-in : Gorgeous Grampa",
    1362657600: "Event : St. Patrick's Day 2013",
    1363262400: "Episode Tie-in : Dark Knight Court",
    1363867200: "Level 27",
    1365422400: "Episode Tie-in : What Animated Women Want",
    1365595200: "Event : Whacking Day 2013",
    1366372800: "Level 28",
    1367496000: "Episode Tie-in : Whiskey Business",
    1368187200: "Level 29",
    1368705600: "Season 24 Yard Sale",
    1369742400: "Gil Offer : Day Old Donuts",
    1370001600: "Level 30",
    1371038400: "Expansion : Squidport",
    1372420800: "Event : July 4th 2013",
    1373889600: "Level 31",
    1374753600: "Level 32",
    1375272000: "Expansion : Krustyland",
    1376568000: "Level 33",
    1377777600: "Level 34",
    1378987200: "Level 35",
    1379937600: "Episode Tie-in : Season Premiere 2013 Homerland",
    1380628800: "Event : Treehouse of Horror XXIV",
    1382529600: "Level 36",
    1383825600: "Level 37",
    1384516800: "Event : Thanksgiving 2013",
    1386158400: "Episode Tie-in : Yellow Subterfuge",
    1386676800: "Event : Christmas 2013",
    1389182400: "Episode Tie-in : Married to the Blob",
    1389873600: "Level 38",
    1390996800: "Event : Super Bowl",
    1391601600: "Event : Valentine's Day 2014",
    1391688000: "Friend Points",
    1394020800: "Episodes Tie-in : Diggs and The Man Who Grew Too Much",
    1394625600: "Event : St. Patrick's Day 2014",
    1395230400: "Episode Tie-in : The War of Art",
    1396008000: "Level 39",
    1397044800: "Episode Tie-in : Days of Future Future",
    1397563200: "Event : Easter 2014",
    1398859200: "Level 40",
    1400155200: "Episode Tie-in : Yellow Badge of Cowardge",
    1400760000: "Level 41",
    1401796800: "Event : Stonecutters",
    1403092800: "Level 42",
    1403784000: "Gil Offer : Mansion of Solid Gold",
    1404302400: "Event : 4th 2014",
    1405598400: "Yard Sale 2014",
    1406116800: "Level 43",
    1407153600: "Gil Offer : Summer Donuts Extravaganza",
    1407931200: "Gil Offer : Back to School",
    1408449600: "Event : Clash of Clones",
    1409918400: "Level 44",
    1410955200: "Level 45",
    1411560000: "Episode Tie-in : Season Premiere 2014 Clown in the Dumps",
    1412251200: "Gil Offer : Shadow Knight",
    1412683200: "Event : Treehouse of Horror XXV",
    1413460800: "Episode Tie-in : Treehouse of Horror XXV",
    1413979200: "Level 46",
    1414584000: "Gil Offer : Ghost Pirate Airship",
    1415188800: "Episode Tie-in : Matt Groening",
    1415793600: "Level 47",
    1416484800: "Event : Thanksgiving 2014 and Episode Tie-in : Covercraft",
    1417176000: "Gil Offer : Black Friday 2014 and Truckasaurus",
    1417608000: "Event : Winter 2014",
    1418731200: "Level 48",
    1421323200: "Gil Offer : Queen Helvetica",
    1421841600: "Episode Tie-in : The Musk Who Fell to Earth",
    1422446400: "Level 49",
    1423137600: "Offer : Stonecutters Black Market Sale",
    1423742400: "Event : Valentine's Day 2015",
    1424260800: "Event : Superheroes",
    1425470400: "Level 50",
    1426075200: "Gil Offer : The Homer",
    1426161600: "Event : St. Patrick's Day 2015",
    1427889600: "Mystery Box Upgrade and Event : Easter 2015",
    1428494400: "Level 51",
    1429012800: "Event : Terwilligers",
    1429704000: "Episode Tie-in : The Kids Are All Fight",
    1430913600: "Level 52 and Money Mountain",
    1431518400: "Episode Tie-in : Mathlete's Feat",
    1432900800: "Level 53",
    1433332800: "Event : Pride Month 2015",
    1434110400: "Gil Offer : End of School",
    1434542400: "Level 54",
    1435060800: "Event : Tap Ball",
    1435665600: "Event : 4th of July 2015",
    1436961600: "Level 55",
    1437566400: "Expansion : Springfield Heights",
    1438257600: "Gil Offer : Ice Cream Man Homer",
    1438862400: "Level 56",
    1439294400: "Event : Monorail",
    1440590400: "Gil Offer : Muscular Marge",
    1441281600: "Level 57",
    1442491200: "Level 58",
    1443009600: "Episode Tie-in : Season Premiere 2015 Every Man's Dream",
    1443096000: "IRS and Job Manager",
    1443614400: "Gil Deal : Oktoberfest",
    1444132800: "Event : Treehouse of Horror 2015",
    1444910400: "Episode Tie-in : Halloween of Horror",
    1445428800: "Episode Tie-in : Treehouse of Horror XXVI",
    1445515200: "New User Power Ups",
    1446033600: "Gil Offer : Halloween Promo",
    1447156800: "Level 59",
    1447934400: "Event : Gobble, Gobble, Toil and Trouble",
    1448539200: "Gil Offer : Black Friday 2015",
    1449057600: "Expansion : Springfield Heights Chapter 2",
    1449576000: "Event : Winter 2015",
    1452686400: "Episode Tie-in : Much Apu About Something",
    1453377600: "Event : Deep Space Homer",
    1454587200: "Gil Offer : Tailgate and Daily Challenges System",
    1455105600: "Event : Valentine's Day 2016",
    1455710400: "World's Largest Redwood",
    1456228800: "Event : Burns' Casino",
    1457524800: "Episode Tie-in : The Marge-ian Chronicles",
    1458129600: "Event : St. Patrick's Day and Easter 2016",
    1459425600: "Event : Crook and Ladder",
    1460548800: "Spring Cleaning",
    1461067200: "Event : Wild West",
    1462795200: "Whacking Day 2016",
    1463572800: "Level 60",
    1464868800: "Event : Homer's Chiliad",
    1465905600: "Event : Superheroes 2",
    1467201600: "Event : 4th of July 2016",
    1469534400: "Dilapidated Rail Yard",
    1470225600: "Event : Springfield Games",
    1471348800: "Event : SciFi",
    1474459200: "Episode Tie-in : Season Premiere 2016 \"Monty Burns\' Fleeing Circus\"",
    1475582400: "Event : Treehouse of Horror XXVII",
    1478692800: "Episode Tie-in : Havana Wild Weekend",
    1479297600: "Event : The Most Dangerous Game",
    1481025600: "Event : Winter 2016",
    1482408000: "First Time Packs",
    1483444800: "Event : Homer the Heretic",
    1484136000: "Episode Tie-in : The Great Phatsby",
    1485345600: "Lunar New Year 2017",
    1485864000: "Event : Destination Springfield",
    1486036800: "Football 2017",
    1486555200: "Valentine\'s 2017",
    1488974400: "Episode Tie-in : 22 for 30",
    1489579200: "Rommelwood Academy",
    1489752000: "St. Patrick\'s 2017",
    1490097600: "Hellfish Bonanza",
    1490702400: "Event : Secret Agents",
    1492344000: "Easter 2017",
    1494417600: "Pin Pals",
    1495627200: "Forgotten Anniversary",
    1496145600: "Event : Time Traveling Toaster, Donut Day 2017 and Road to Riches",
    1498651200: "4th of July 2017",
    1499256000: "Pride 2017",
    1500465600: "Superheroes Return",
    1501070400: "Stunt Cannon",
    1501588800: "Event : Homerpalooza",
    1505217600: "County Fair",
    1506513600: "Episode Tie-in : The Serfsons",
    1506600000: "Classic Mini Events and Monorail Promo",
    1507032000: "Event : Treehouse of Horror XXVIII",
    1510660800: "This Thanksgiving\'s Gone to the Birds!",
    1511956800: "A Rigellian Christmas Promo",
    1512475200: "Event : The Invasion Before Christmas",
    1514980800: "Event : The Buck Stops Here and Episode Tie-In : \"Haw-Haw Land\"",
    1516190400: "Classic Mini Events and Bart Royale Teaser",
    1516708800: "Event : Bart Royale",
    1518436800: "Valentine\'s Day 2018",
    1520424000: "Event : Homer vs the 18th Amendment",
    1521028800: "The Springfield Jobs Teaser",
    1521115200: "Episode Tie-In : \"Homer is Where the Art Isn\'t\"",
    1521633600: "Event : The Springfield Jobs",
    1525867200: "Event : Who Shot Mr. Burns? (Part Three)",
    1527076800: "Bart the Fink",
    1527681600: "Event : Itchy & Scratchy Land",
    1529323200: "Pride 2018",
    1530100800: "July 4th 2018",
    1531310400: "Event : Poochie\'s Dog Dayz",
    1532520000: "Moe\'s Ark Teaser",
    1533124800: "Event : Moe's Ark",
    1536753600: "Event : Super Powers",
    1537963200: "Treehouse of Horror XXIX Teaser",
    1538568000: "Event : Treehouse of Horror XXIX",
    1539777600: "Episode Tie-In : \"Treehouse of Horror XXIX\"",
    1542196800: "Event : Thanksgiving 2018",
    1542801600: "Black Friday 2018",
    1543406400: "A Simpsons Christmas Special Teaser",
    1543924800: "Event : A Simpsons Christmas Special",
    1547035200: "Event : Not Yet Spring Cleaning",
    1548244800: "Event : Love, Springfieldian Style",
    1551268800: "Event : State of Despair",
    1552478400: "Event : A Classless Reunion",
    1554897600: "Marge at the Bat",
    1557921600: "Event : The Real Moms of Springfield",
    1560340800: "Event : Game of Games",
    1562155200: "4th of July 2019",
    1563364800: "Event : Flanders Family Reunion",
    1565179200: "Event : Simpsons Babies",
    1568203200: "Event : Krusty's Last Gasp Online",
    1569499200: "Event : Cthulhu's Revenge",
    1571832000: "Event : Treehouse of Horror XXX",
    1574251200: "Event : All American Auction and Black Friday 2019",
    1576065600: "Event : Abe's in Toyland",
    1579089600: "Event : Holidays of Future Past",
    1580904000: "Event : Black History",
    1582891200: "Share the Magic",
    1584532800: "Event : No Bucks Given",
    1586347200: "Event : Simpsons Wrestling",
    1589976000: "Event : Pride 2020",
    1591790400: "Event : Game of Games The Sequel",
    1595592000: "Event : Summer Games 2020",
    1597233600: "Event : The Van Houtens",
    1600862400: "Event : All This Jazz",
    1602676800: "Event : Treehouse of Horror XXXI",
    1605700800: "Event : Blargsgiving",
    1607428800: "Event : Clash of Creeds: Christmas Royale",
    1611144000: "Event : New Year New You",
    1612353600: "Event : Love and War",
    1615982400: "Event : Springfield Choppers",
    1617796800: "Event : Rise of the Robots",
    1621425600: "Event : Foodie Fight",
    1623240000: "Event : Springfield Enlightened",
    1626264000: "Event : Tavern Trouble",
    1628078400: "Event : Into the Simpsonsverse",
    1631707200: "Event : Breakout Bounty",
    1632744000: "Event : Season 33 Prize Track",
    1633521600: "Event : Treehouse of Horror XXXII",
    1637150400: "Event : Northward Bound",
    1638964800: "Event : Holiday Whodunnit",
    1642593600: "Event : Red Alert",
    1644408000: "Event : Cirque du Springfield",
    1648036800: "Event : Hot Diggity D'oh!",
    1649851200: "Event : Hell on Wheels",
    1653480000: "Event : When the Bough Breaks",
    1655294400: "Event : Dog Days",
    1658923200: "Event : Splash and Burn",
    1660132800: "Event : Showbiz Showdown",
    1660651200: "Tenth Anniversary",
    1663761600: "Event : Tragic Magic",
    1665576000: "Event : Treehouse of Horror XXXIII",
    1668600000: "Event : The Atom Smasher",
    1670414400: "Event : A Christmas Peril",
    1674043200: "Event : Springfield's Got Talent?",
    1675857600: "Event : A Warmfyre Welcome",
    1679486400: "Event : Shiver Me Barnacles",
    1681300800: "Event : Heaven Won't Wait",
    1684324800: "Event : For All Rich Mankind",
    1686139200: "Event : Fore!",
    1689163200: "Event : Food Wars",
    1690977600: "Event : Mirror Mayhem",
    1694606400: "Event : A Hard Play's Night",
    1696420800: "Event : Treehouse of Horror XXXIV",
    1700049600: "Event : Cold Turkey",
    1701864000: "Event : Snow Place Like the Woods",
    1705492800: "Event : Better Late Than Forever",
    1707307200: "Event : Fears of a Clown",
    1707739200: "Valentine's 2024 Promotion",
    1710936000: "Event : The Great Burnsby",
    1712750400: "Event: The Simpsanos",
    1716379200: "Event : Charity Case",
    1717588800: "Event : Summer of Our Discontent",
    1717675200: "Donut Day 2024 Promotion",
    1719835200: "4th of July 2024 Promotion",
    1721217600: "Event : The Mayflower Maple Bowl",
    1723032000: "Event : A Bart Future",
    1727352000: "Final Event : Taps"
}


class TheSimpsonsTappedOutLocalServer:
  
  def __init__(self):
    # Generate the Flask application object
    self.app = Flask(__name__)
    Inflate(self.app)     # Enable ability to auto decompress gzip responses

    # Load the configuration first
    self.CONFIG_FILENAME = "config.json"
    self.config: dict = {}
    self.load_config()

    # Set the initial configuration
    self.server_url: str = self.config.get("server_url") if len(sys.argv) != 2 else sys.argv[1]
    self.debug: bool = self.config.get("debug", False)
    self.http_debug: bool = self.config.get("http_debug", False)
    self.run_tutorial: bool = self.config.get("debug", False)
    self.dlc_dir: str = self.config.get("dlc_dir")
    self.towns_dir: str = self.config.get("towns_dir")
    self.town_filename: str = self.config.get("active_town")
    self.reverse_proxy: bool = self.config.get("reverse_proxy", False)
    self.secret_key: str = self.config.get("secret_key", "")
    self.client_secret: str = self.config.get("client_secret", "")

    friends_config: dict = self.config.get("friends_config", {})
    self.randomize_friends: bool = friends_config.get(
                                                      "randomize_friends", 
                                                      False
                                                     )
    self.max_friends: int = friends_config.get("max_friends", 5)
    self.force_friend_name_order_randomization: bool = friends_config.get(
                                        "force_friend_name_order_randomization",
                                        False
                                      )

    # Construct the various authenticator personas
    self.auth_base_id = self.config.get("identity_info", {}).get("auth_base_id")
    self.anon_base_id = self.config.get("identity_info", {}).get("anon_base_id")
    self.intm_base_id = self.config.get("identity_info", {}).get("intm_base_id")

    self.id_multiplier = self.config.get("identity_info", {}).get("multiplier")
    self.persona_base = self.config.get("identity_info", {}).get("persona_base")
    self.authenticator_base = self.config.get(
                                              "identity_info", {}
                                             ).get(
                                                "authenticator_base",
                                                10070000000000
                                              )

    self.auth_id_set = self.generate_persona_id_set(
                                                    "NUCLEUS",
                                                    self.auth_base_id
                                                   )
    self.anon_id_set = self.generate_persona_id_set(
                                                    "ANONYMOUS",
                                                    self.anon_base_id
                                                   )
    self.intm_id_set = self.generate_persona_id_set(
                                                    "ANONYMOUS", 
                                                    self.intm_base_id
                                                   )

    # Establish a play mode offset (for switching events)
    self.time_offset: int = 0
    self.current_play_mode: int = 0   # if non-zero, the timestamp for an event
    
    self.set_game_mode(0)

    self.session_key: str = str(uuid.uuid4())
    self.land_token: str = str(uuid.uuid4())
    self.user_user_id: str = "".join([chr(random.randint(0x30,0x39)) for x in range(38)])
    self.user_telemtry_id: str = "".join([chr(random.randint(0x30,0x39)) for x in range(11)])
    self.token_session_key: str = hashlib.md5(str(datetime.datetime.now()).encode("utf-8")).hexdigest()
    self.token_user_id: str = str(random.randint(1000000000000, 99999999999999))
    self.token_authenticator_pid_id: int = random.randint(1000000000000, 99999999999999)
    self.token_persona_id: int = random.randint(1000000000000, 99999999999999)
    self.token_telemetry_id: str = str(random.randint(1000000000000, 99999999999999))
    self.personal_id: str = str(random.randint(1000000000000, 99999999999999))
    self.display_name: str = ''.join(random.choice(string.ascii_uppercase + string.ascii_lowercase) for _ in range(random.randint(5,12)))
    self.persona_name: str = self.display_name.lower()
    self.me_persona_pid_id: str = str(random.randint(1000000000000, 99999999999999))
    self.me_persona_display_name: str = ''.join(random.choice(string.ascii_uppercase + string.ascii_lowercase) for _ in range(random.randint(5,12)))
    self.me_persona_name: str = self.me_persona_display_name.lower()
    self.me_persona_anonymous_id: str = base64.b64encode(hashlib.md5(self.me_persona_name.encode("utf-8")).digest()).decode("utf-8")
    self.device_id: str = hashlib.sha256(str(datetime.datetime.now()).encode("utf-8")).hexdigest()
    self.auth_code: str = ""


    # Generate a set of identity identifiers (user_ids, token_ids, pid_ids)
    anon_base_id: int = 0
    user_base_id: int = 0


    # Generate a blank world (LandMessage) then attempt to load the active town
    self.land_proto: LandData_pb2.LandMessage = LandData_pb2.LandMessage()
    self.load_town()
    
    # Load all towns' friendData
    self.friends_data: dict = {}    # dictionary of FriendDataPair protos by id
    self.load_friends_data()

    # if behind a reverse proxy setup so that the correct IP is recorded
    if self.reverse_proxy:
      self.app.wsgi_app = ProxyFix(
        self.app.wsgi_app, x_for=1, x_proto=1, x_host=1, x_prefix=1
      )
    
    # Add the URL routes for Flask
    self.add_routes()

  ##############################################################################
  # Config management
  ##############################################################################

  def load_config(self) -> bool:
    """Load server configuration from json file"""
    self.config = {}
    with open(self.CONFIG_FILENAME, "r") as f:
      self.config = json.load(f)
    return True
    
  def save_config(self) -> bool:
    """Save server configuration to json file"""
    with open(self.CONFIG_FILENAME, "w") as f:
      json.save(self.config, f)
    return True

  ##############################################################################
  # Persona management
  ##############################################################################
  def generate_persona_id_set(self, set_type: str, id_base: int = 0) -> dict:
    """Generates a new set of pid/persona/user IDs"""
    
    # The pattern for pid/user_id/persona_id sets is to have a large base
    # number of somewhere between 1007000000000 and 1009999999999 for your
    # persona_id. The pid_id and user_id fields exist within the range of
    # 1010000000000 and 1029999999999. The lower last five digits of each
    # _id set match. This is the token_base value. The difference between 
    # pid_id and user_id is always 200k with the user_id always being the 
    # smaller of the two number. The difference between the persona_id and the
    # user_id varies but is always some multiple of 200k. The numbers do not 
    # appear to be sequential based on making multiple requests to the server.
 
    persona_id = self.persona_base + id_base
    if set_type != "NUCLEUS":       # only "ANONYMOUS" and "NUCLEUS" are valid
      persona_id = self.authenticator_base + id_base

    # Generate the user_id by making a random offset from persona_id. The min
    # offset is 3,000,000,000 and the max is 23,999,999,999 but it needs to be
    # in a multiple of 200,000 (meaning a 15,000 to 120,000 range)
    offset = self.id_multiplier * 200000
    user_id = persona_id + offset
    
    # Generate the pid_id by offset the pid by 200000
    pid_id = user_id + 200000

    # return the set
    id_set = {
      "persona_id": persona_id,
      "user_id": user_id,
      "pid_id": pid_id
    }

    self.log_debug(f"new persona set generated: {id_set}")
    return id_set



  ##############################################################################
  # Town file management
  ##############################################################################

  def load_town(self) -> bool:
    """Attempts to load the current town_filename."""
    
    self.log_debug(f"Attempting to load {self.town_filename}")

    # If the town_filename is not specified, just use a blank world and
    # set the run_tutorial to true.
    if not self.town_filename:
      # This is for loading a blank/tutorial town
      self.log_debug("No town found. Generating blank world.")
      self.land_proto: LandData_pb2.LandMessage = LandData_pb2.LandMessage()
      self.land_proto.friendData.dataVersion = 72
      self.land_proto.friendData.hasLemonTree = False
      self.land_proto.friendData.language = 0
      self.land_proto.friendData.level = 0
      self.land_proto.friendData.name = "tsto_server_blank"
      self.land_proto.friendData.rating = 0
      self.land_proto.friendData.boardwalkTileCount = 0
      self.run_tutorial = True
      self.log_debug(f"[r]land_proto is {len(self.land_proto.SerializeToString())} bytes")

      return True
    
    # Try to load the specified world from disk
    town_file_path = os.path.join(self.towns_dir, self.town_filename)
    if not os.path.isfile(town_file_path):
      # Unable to find the file, so generate a blank town and save it
      self.log_debug(f"{town_file_path} wasn't found")
      self.land_proto: LandData_pb2.LandMessage = LandData_pb2.LandMessage()
      self.land_proto.friendData.dataVersion = 72
      self.land_proto.friendData.hasLemonTree = False
      self.land_proto.friendData.language = 0
      self.land_proto.friendData.level = 0
      self.land_proto.friendData.name = "tsto_server_blank"
      self.land_proto.friendData.rating = 0
      self.land_proto.friendData.boardwalkTileCount = 0
      self.run_tutorial = True
      self.log_debug(f"[r]land_proto is {len(self.land_proto.SerializeToString())} bytes")
      return self.save_town()

    # Try to load the specified town...
    with open(town_file_path, "rb") as f:
      # Check if we have a teamtsto.org backup.
      try:
        self.land_proto.ParseFromString(f.read())
      except google.protobuf.message.DecodeError:
        try:
          f.seek(0x0c)      # see if this might be a teamtsto.org backup
          self.land_proto.ParseFromString(f.read())          
        except google.protobuf.message.DecodeError:
          self.log_error("Unable to load {self.town_filename} town.")
          return False
      if self.land_proto.HasField("id"):
        self.user_user_id = self.land_proto.id
      proto_size = len(self.land_proto.SerializeToString())
      self.log_debug(f"[r]land_proto is {proto_size} bytes") 
      self.log_debug(f"   land id is now {self.user_user_id}")     

    return True

  def save_town(self) -> bool:
    """Save the current .land_proto to disk as .town_filename."""
    # If there somehow isn't an active .land_proto, return error.
    if not self.land_proto:
      self.log_error(f".land_proto is None. Unable to save.")
      return False
    
    # If there isn't a name for the town, return error.
    if not self.town_filename:
      self.log_error(f".town_filename is None. Unable to save.")
      return False

    # Construct the full path/name
    town_file_path = os.path.join(self.towns_dir, self.town_filename)
    with open(town_file_path, "wb") as f:
      f.write(self.land_proto.SerializeToString())
      self.log_debug(f"land_proto is {len(self.land_proto.SerializeToString())} bytes")


    return True

  ##############################################################################
  # Friend management
  ##############################################################################

  def load_friends_data(self) -> bool:
    """Loads all of the LandMessage.FriendData data from the known towns."""

    # TODO: if you have an excessive number of towns this could be slow! fix.

    # iterate through all of the town protobufs in the towns directory
    for town_filename in os.listdir(self.towns_dir):
      town_file_path = os.path.join(self.towns_dir, town_filename)
      if not os.path.isfile(town_file_path):
        self.log_debug("skipping {town_file_path}: not found.")
        continue

      friend_land_proto: LandData_pb2.LandMessage = LandData_pb2.LandMessage()

      with open(town_file_path, "rb") as f:
        # Check if we have a teamtsto.org backup.
        try:
          friend_land_proto.ParseFromString(f.read())
        except google.protobuf.message.DecodeError:
          try:
            f.seek(0x0c)      # see if this might be a teamtsto.org backup
            friend_land_proto.ParseFromString(f.read())          
          except google.protobuf.message.DecodeError:
            self.log_error("Unable to load {self.town_filename} town.")
            continue

        # Validate necessary fields
        if not friend_land_proto.HasField("id"):
          self.log_debug("{town_filename}: missing id field. skipping.")
          continue
        if not friend_land_proto.HasField("friendData"):
          self.log_debug("{town_filename}: missing friendData field. skipping.")
          continue
        if not friend_land_proto.friendData.HasField("name"):
          self.log_debug(
            "{town_filename}: missing friendData.name field. skipping."
          )
          continue

        friend_id: str  = friend_land_proto.id
        friend_data = GetFriendData_pb2.GetFriendDataResponse.FriendDataPair()
        friend_data.friendId = friend_id
        friend_data.friendData.CopyFrom(friend_land_proto.friendData)
        self.friends_data[friend_id] = friend_data
        
    return True

  ##############################################################################
  # Response helpers
  ##############################################################################
  def error_invalid_request(self, 
                              description: str,
                              code_num: int = None,
                              status: int = 400):
    """Aborts with invalid_request error response"""
    response_dict = {
          "error": "invalid_request"
    }
    
    if description:
      response_dict["error_description"] = description
    if code_num is not None:
      response_dict["code"] = code_num

    abort(make_response(jsonify(response_dict), status))

  def error_invalid_client(self, 
                              description: str,
                              code_num: int = None,
                              status: int = 400):
    """Aborts with invalid_client error response"""
    response_dict = {
          "error": "invalid_client"
    }
    
    if description:
      response_dict["error_description"] = description
    if code_num is not None:
      response_dict["code"] = code_num

    abort(make_response(jsonify(response_dict), status))

  def error_invalid_oauth_info(self):
    """Aborts with an invalid_oauth_info error response"""
    abort(make_response(jsonify({"error": "invalid_oauth_info"}), 400))
    
  ##############################################################################
  # Common header/arg validators/gets
  ##############################################################################

  def validate_client_id(self) -> str:
    """Validate's client_id param. Aborts on error. Returns value."""
    client_id = request.args.get("client_id")
    if not client_id:
      self.log_debug("Missing client_id")
      self.error_invalid_request("client_id is missing", 101102)
    
    if client_id not in ["simpsons4-android-client", "simpsons4-ios-client"]:
      self.log_debug("Invalid client_id: {client_id}")
      self.error_invalid_client("client_id is invalid", 101102)

    return client_id

  def validate_authorization_header(self) -> str:
    """Validate Authorization header, aborts on error, else returns auth code"""
    authorization = request.headers.get("Authorization")
    if not authorization:
      self.log_debug("Missing authorization header")
      self.error_invalid_oauth_info()
    
    # break the authorization into its Bearer and token components
    auth_header_parts = authorization.split(" ")
    if len(auth_header_parts) != 2:
      self.log_debug("Invalid authorization header: {authorization}")
      self.error_invalid_oauth_info()
    
    bearer, auth_token = auth_header_parts
    if not self.auth_manager.is_access_token_valid(auth_token):
      self.log_debug("Unknown authorization token: {auth_token}")
      self.error_invalid_oauth_info()
      
    return auth_token

  def validate_redirect_uri(self) -> str:
    """Validates redirect_uri param, aborts on error, else returns value."""
    redirect_uri = request.args.get("redirect_uri")

    # no error if it is missing
    if redirect_uri and redirect_uri != "nucleus:rest":
      self.log_debug("redirect_uri is invalid")
      self.error_invalid_request("redirect_uri is invalid", 102111)

    return redirect_uri

  def validate_client_secret(self) -> str:
    """Validates client_secret, aborts on error, else returns value."""

    client_secret = request.args.get("client_secret")
    if not client_secret:
      self.log_debug(f"Missing client_secret")
      self.error_invalid_client("unknown client", 101102)

    if client_secret != self.config.get("client_secret"):
      self.log_debug("Invalid client_secret: {client_secret}")
      self.error_invalid_client("authentication failed", 101108)

    return client_secret

  ##############################################################################
  # Server management
  ##############################################################################

  def add_routes(self):
    """Add the routes and map their handler functions."""
    
    # server: syn-dir.sn.eamobile.com
    self.app.add_url_rule("/director/api/<string:platform>/getDirectionByPackage",
                            view_func = self.get_direction_by_bundle)  # android
    self.app.add_url_rule("/director/api/<string:platform>/getDirectionByBundle",
                            view_func = self.get_direction_by_bundle)  # ios

    # server: oct2018-4-35-0-uam5h44a.tstodlc.eamobile.com
    self.app.add_url_rule(# "/netstorage/gameasset/direct/simpsons/dlc/<string:filename>",
                          "/gameassets/<string:directory>/<string:filename>",
                            view_func = self.dlc_download)

    # server: user.sn.eamobile.com
    self.app.add_url_rule("/user/api/android/validateDeviceID",
                            view_func = self.validate_device_id)
    self.app.add_url_rule("/user/api/android/getDeviceID",
                            view_func = self.get_device_id)
    self.app.add_url_rule("/user/api/android/getAnonUid",
                            view_func = self.get_anon_uid)

    # server: gateway.ea.com
    self.app.add_url_rule("/proxy/identity/pids/me/personas/<string:user_id>",
                            view_func = self.me_personas)
    self.app.add_url_rule("/proxy/identity/pids/<string:pid>/personas",
                            view_func = self.pid_personas)
    self.app.add_url_rule("/proxy/identity/progreg/code", methods=["POST"],
                            view_func = self.progreg_code)

    # server: accounts.ea.com
    self.app.add_url_rule("/connect/auth", view_func = self.connect_auth)
    self.app.add_url_rule("/connect/token", methods=["POST"],
                            view_func = self.connect_token)
    self.app.add_url_rule("/connect/tokeninfo",
                            view_func = self.connect_tokeninfo)
    self.app.add_url_rule("/proxy/identity/geoagerequirements",
                            view_func = self.geoagerequirements)
    self.app.add_url_rule("/probe", view_func = self.probe)

    # server: pn.tnt-ea.com
    self.app.add_url_rule("/rest/v1/games/<game_id>/devices", methods=["POST"],
                            view_func = self.game_devices)
    self.app.add_url_rule("/games/<game_id>/devices", methods=["POST"],
                            view_func = self.game_devices)
   
    # server river-mobile.data.ea.com
    self.app.add_url_rule("/tracking/api/core/logEvent", methods=["POST"],
                            view_func = self.core_logEvent)
  
    # server: pin-river.data.ea.com
    self.app.add_url_rule("/pinEvents", methods=["POST"],
                            view_func = self.pinEvents)

    # server: prod.simpsons-ea.com
    self.app.add_url_rule("/mh/gameplayconfig", view_func = self.gameplayconfig)
    self.app.add_url_rule("/mh/games/bg_gameserver_plugin/trackinglog/",
                            methods=["POST"], view_func = self.tracking_log)
    self.app.add_url_rule("/mh/games/bg_gameserver_plugin/protoClientConfig/",
                            view_func = self.protoClientConfig)
    self.app.add_url_rule("/mh/games/bg_gameserver_plugin/protoWholeLandToken/<land_id>/", 
                            methods=["POST"],
                            view_func = self.protoWholeLandToken)
    self.app.add_url_rule("/mh/games/bg_gameserver_plugin/checkToken/<token>/protoWholeLandToken/",
                            view_func = self.checkToken)
    self.app.add_url_rule("/mh/games/bg_gameserver_plugin/trackingmetrics/",
                            methods=["POST"], view_func = self.trackingmetrics)
    self.app.add_url_rule("/mh/games/bg_gameserver_plugin/protoland/<land_id>/",
                            methods=["POST", "GET"],
                            view_func = self.bg_gameserver_plugin_protoland)
    self.app.add_url_rule("/mh/games/bg_gameserver_plugin/protocurrency/<land_id>/",
                            view_func = self.protocurrency)
    self.app.add_url_rule("/mh/games/bg_gameserver_plugin/extraLandUpdate/<land_id>/protoland/",
                            methods=["POST"],
                            view_func = self.extraLandUpdate)
    self.app.add_url_rule("/mh/games/bg_gameserver_plugin/event/<land_id>/protoland/",
                            view_func = self.plugin_event)
    self.app.add_url_rule("/mh/games/bg_gameserver_plugin/friendData",
                            methods=["POST"], view_func = self.friend_data)
    self.app.add_url_rule("/mh/games/bg_gameserver_plugin/friendData/origin",
                            methods=["GET"], view_func = self.friend_data)
    self.app.add_url_rule("/mh/games/lobby/time", view_func = self.lobby_time)
    self.app.add_url_rule("/mh/users", methods=["PUT", "GET"],
                            view_func = self.mh_user)
    self.app.add_url_rule("/mh/userstats", methods=["POST"],
                            view_func = self.userstats)

    # server: any (generic rule)

    self.app.add_url_rule("/", view_func = self.default_root)

    # server: local 
    
    self.app.add_url_rule("/dashboard", view_func = self.dashboard)
    self.app.add_url_rule("/controller/api/update", methods=["POST"],
                            view_func = self.controller_update)

  def run(self):
    """Activates the Flask server."""
    
    # start http server
    http_debug = self.config.get("http_debug", False)
    listening_ip = self.config.get("listening_ip", "0.0.0.0")
    listening_port = int(self.config.get("listening_port", 9000))
    self.app.run(debug=http_debug, host=listening_ip, port=listening_port)

  ##############################################################################
  # Utility functions
  ##############################################################################

  def log_debug(self, msg: str, frame_depth = 1):
    """Prints msg to stdout if debug set with function info prepended."""
    # If this is called by a child function (e.g. print_args) then the
    # frame_depth should be set to 2 so that the caller's info is used
    if self.debug:
      caller_class = self.__class__.__name__
      caller_name = sys._getframe(frame_depth).f_code.co_name
      print(f"[DEBUG:{caller_class}.{caller_name}] {msg}")

  def log_error(self, msg: str, frame_depth = 1):
    """Prints msg to stdout always"""
    # If this is called by a child function (e.g. print_args) then the
    # frame_depth should be set to 2 so that the caller's info is used
    caller_class = self.__class__.__name__
    caller_name = sys._getframe(frame_depth).f_code.co_name
    print(f"[ERROR:{caller_class}.{caller_name}] {msg}")

  def log_http_debug(self, msg: str, frame_depth = 1):
    """Prints msg to stdout if debug set with function info prepended."""
    # If this is called by a child function (e.g. print_args) then the
    # frame_depth should be set to 2 so that the caller's info is used
    if self.http_debug:
      caller_class = self.__class__.__name__
      caller_name = sys._getframe(frame_depth).f_code.co_name
      print(f"[{caller_class}.{caller_name}] {msg}")

  def print_headers(self):
    """Prints the request headers"""
    if self.http_debug:
      self.log_http_debug("Request Headers:", 2)
      pprint.pprint(request.headers)

  def print_args(self):
    """Prints the query params"""
    if self.http_debug:
      self.log_http_debug("Request Query Params:", 2)
      pprint.pprint(request.args)

  def print_response(self, response):
    """Prints the response and response headers"""
    if self.http_debug:
      if hasattr(response, "headers"):
        self.log_debug("Response Headers:", 2)
        pprint.pprint(response.headers)
      self.log_debug("Response:", 2)
      print(response.get_data(True))

  def fix_pad(self, s: str) -> str:
    """Returns base64 string with proper padding."""
    pad_size = (4 - (len(s) % 4)) & 3
    s += "=" * pad_size
    return s

  def generate_random_bytes(self, length: int) -> bytes:
    """Generates a random sequence of bytes of length."""
    array = bytearray(length)
    for i in range(length):
      array[i] = random.randint(0,255)
    return array

  def now_int(self) -> int:
    """Returns the current time (UTC) as unix timestamp integer."""
    ts = int(datetime.datetime.now(datetime.timezone.utc).timestamp())
    ts -= self.time_offset
    return ts

  def set_game_mode(self, new_time: int):
    """Adjust the time_offset based on the desired new_time"""
    if not new_time:
      # reset the time to now.
      self.time_offset = 0
      self.current_play_mode = 0
    else:    
      # Calculate the time delta value
      now = int(datetime.datetime.now(datetime.timezone.utc).timestamp())
      self.time_offset = now - new_time

    self.current_play_mode = new_time


  ##############################################################################
  # Server route handlers
  ##############################################################################

  # server: syn-dir.sn.eamobile.com
  
  def get_direction_by_bundle(self, platform: str):
    """Handler for the URLs:
      syn-dir.sn.eamobile.com/director/api/<string:platform>/getDirectionByPackage
      syn-dir.sn.eamobile.com/director/api/<string:platform>/getDirectionByBundle
    """
    self.print_headers()
    self.print_args()

    resp = {
              "DMGId": 0,
              "appUpgrade": 0,
              "bundleId": "com.ea.simpsonssocial.inc2",
              "clientId": f"simpsons4-{platform}-client",
              "clientSecret": "D0fpQvaBKmAgBRCwGPvROmBf96zHnAuZmNepQht44SgyhbCdCfFgtUTdCezpWpbRI8N6oPtb38aOVg2y",
              "disabledFeatures": [],
              "facebookAPIKey": "43b9130333cc984c79d06aa0cad3a0c8",
              "facebookAppId": "185424538221919",
              "hwId": 2363,
              "mayhemGameCode": "bg_gameserver_plugin",
              "mdmAppKey": f"simpsons-4-{platform}",
              "millennialId": "",
              "packageId": "com.ea.game.simpsons4_row",
              "pollIntervals": [
                  {
                      "key": "badgePollInterval",
                      "value": "300"
                  }
              ],
              "productId": 48302,
              "resultCode": 0,
              "sellId": 857120,
              "serverApiVersion": "1.0.0",
              "serverData": [
                  {
                      "key": "antelope.rtm.host",
                      "value": f"{self.server_url}"
                  },
                  {
                      "key": "applecert.url",
                      "value": "https://www.apple.com/appleca/AppleIncRootCertificate.cer"
                  },
                  {
                      "key": "origincasualapp.url",
                      "value": f"{self.server_url}/loader/mobile/ios/"
                  },
                  {
                      "key": "akamai.url",
                      "value": f"{self.server_url}/skumasset/gameasset/" #"http://cdn.skum.eamobile.com/skumasset/gameasset/"
                  }
              ],
              "telemetryFreq": 300
          }

    # Setup URL redirects to our server
    for key in self.config["server_redirects"]:
      resp["serverData"].append({"key": key, "value": f"{self.server_url}"})

    return jsonify(resp)


  # server: oct2018-4-35-0-uam5h44a.tstodlc.eamobile.com

  def dlc_download(self, directory: str, filename: str):
    """Handler for oct2018-4-35-0-uam5h44a.tstodlc.eamobile.com/netstorage/gameasset/direct/simpsons/<string:directory>/<string:filename>"""
    try:
      directory = secure_filename(directory)    # Sanitize the directory
      filename = secure_filename(filename)      # Sanitize the filename
      file_path = os.path.join(self.dlc_dir, directory, filename)
      self.log_debug(f"requested file: {file_path}")
      if os.path.isfile(file_path):
        self.log_debug(f"Sending {file_path} as application/zip")
        return send_file(file_path, mimetype="application/zip")#, as_attachment=True)
      else:
        self.log_error(f"File '{filename}' not found")
        return make_response(f"File '{filename}' not found.", 404)
    except Exception as e:
      self.log_error(f"Exception: {e}")
      return make_response(f"Error: {str(e)}", 500) 


  # server: user.sn.eamobile.com

  def get_anon_uid(self):
    """Handler for user.sn.eamobile.com/user/api/android/getAnonUid"""
    self.print_headers()
    self.print_args()

    return jsonify(
      {
        "resultCode": 0,
        "serverApiVersion": "1.0.0",
        "uid": random.randint(10000000000, 99999999999)     # 17987306517
      }
    )

  def get_device_id(self):
    """Handler for user.sn.eamobile.com/user/api/android/getDeviceID"""
    self.print_headers()
    self.print_args()

    return jsonify(
      {
        "deviceId": self.device_id,
        "resultCode": 0,
        "serverApiVersion": "1.0.0"
      }
    )

  def validate_device_id(self):
    """Handler for user.sn.eamobile.com/user/api/android/validateDeviceID"""
    self.print_headers()
    self.print_args()

    return jsonify(
        {
            "deviceId": self.device_id,
            "resultCode": 0,
            "serverApiVersion": "1.0.0"
        }
      )

  # server: gateway.ea.com

  def me_personas(self, user_id: str):
    """Handler for gateway.ea.com/proxy/identity/pids/me/personas/<user_id>"""
    self.print_headers()
    self.print_args()

    return jsonify(
      {
          "persona": {
              "personaId": user_id,
              "pidId": self.me_persona_pid_id,
              "displayName": self.me_persona_display_name,
              "name": self.me_persona_name,
              "namespaceName": "gsp-redcrow-simpsons4",
              "isVisible": True,
              "status": "ACTIVE",
              "statusReasonCode": "",
              "showPersona": "EVERYONE",
              "dateCreated": "2012-02-29T0:00Z",
              "lastAuthenticated": "2024-12-28T5:25Z",
              "anonymousId": self.me_persona_anonymous_id
          }
      }
    )

  def pid_personas(self, pid: str):
    """Handler for gateway.ea.com/proxy/identity/pids/<pid>/personas"""
    self.print_headers()
    self.print_args()

    json = {
      "personas": {
          "persona": [
              {
                  "personaId": self.personal_id,    # TODO: make configurable
                  "pidId": pid,
                  "displayName": self.display_name, # TODO: make configurable
                  "name": self.persona_name,        # TODO: make configurable
                  "namespaceName": "cem_ea_id",
                  "isVisible": True,
                  "status": "ACTIVE",
                  "statusReasonCode": "",
                  "showPersona": "FRIENDS",
                  "dateCreated": "2024-12-25T0:00Z",    # TODO: make configurable
                  "lastAuthenticated": ""
              }
          ]
      }
    }

    return jsonify(json)

  def progreg_code(self):
    """Handler for gateway.ea.com/proxy/identity/progreg/code"""
    self.print_headers()
    self.print_args()

    # Try to parse the request data
    request_data = {}
    try:
      request_data = json.loads(request.data)
    except Exception as e:
      self.log_error(f"Failure to jsonify request: {request.data}")
      abort(500)
    
    filename: str = None
    code_type: str = request_data["codeType"]
    if code_type == "EMAIL":
      filename = request_data["email"]
      # town_filename is empty, fill in with a dummy name
      if not filename:
        filename = "blank_name@email.com"
      # Reduce the email to a filename by stripping everything after @
      filename = filename.split("@")[0]

    elif code_type == "SMS":
      filename = request_data["phoneNumber"]

    else:
      printf("Unknown codeType: {request_data}")
      abort(500)

    self.town_filename = filename

    # Attempt to load the town
    self.load_town()

    return ""

  def friend_data(self):
    """Handler for gateway.simpsons-ea.com/mh/games/bg_gameserver_plugin/friendData[/origin]"""
    self.print_headers()
    self.print_args()
    
    # TODO: if you have an excessive number of towns this could be slow!
    # Get the list of IDs to use based on max count and randomization
    friend_ids: list = [id for id in self.friends_data.keys()]
    if self.randomize_friends:
      random.shuffle(friend_ids)
      
    # Strip the list to the max
    friend_ids = friend_ids[:self.max_friends]
    
    # Start the response protobuffer
    friend_data_response = GetFriendData_pb2.GetFriendDataResponse()
    
    # Add in the friends
    for fid in friend_ids:
      data = self.friends_data.get(fid)
      if not data:
        self.log_debug("{fid} not found!")
        continue
        
      if not isinstance(
              data, 
              GetFriendData_pb2.GetFriendDataResponse.FriendDataPair
             ):
        self.log_debug("{fid} is wrong data type: {type(data)}")
        continue
         
      new_friend = friend_data_response.friendData.add()
      new_friend.CopyFrom(data)

    # Randomize names?
    if self.force_friend_name_order_randomization:
      id_order = [i for i in range(len(friend_data_response.friendData))]
      for i in range(len(friend_data_response.friendData)):
        name = friend_data_response.friendData.friendData.name[i]
        name = f"{id_order[i]}{name}"
        friend_data_response.friendData[i].friendData.name = name
    
    # Make it a response and ship it off to the client
    response = make_response(friend_data_response.SerializeToString())
    response.headers['Content-Type'] = 'application/x-protobuf'
    return response  

  # server: accounts.ea.com

  # TODO: remove this after we fix the new connect_auth
  def connect_auth(self):
    """Handler for accounts.ea.com/connect/auth"""
    """
    ?client_id=simpsons4-ios-client
    &country=US
    &dob=1992-02-01
    &email=dude%40dude.dude
    &globalOptin=true
    &language=en
    &platform=iOS
    &prog_reg=true
    &redirect_uri=nucleus%3Arest
    &release_type=prod
    &response_type=code

    For logins, the verify code is sent using these parameters...
    authenticator_login_type=mobile_ea_account
    &client_id=simpsons4-android-client
    &nonce=2024-12-24%2004:30:37:000
    &redirect_uri=nucleus:rest
    &release_type=prod
    &response_type=code%20lnglv_token
    &sig=ewogICAiYXYiIDogInYxIiwKICAgImNyZWQiIDogIjExMTExMSIsCiAgICJlbWFpbCIgOiAiZHVkZUBkdWRlLmR1ZGUiLAogICAibSIgOiAiMSIsCiAgICJzdiIgOiAidjEiLAogICAidHMiIDogIjIwMjQtMTItMjQgMDQ6MzA6Mzc6MDAwIgp9Cg.RaJxDRmKhLZEtk98tYG8_E9gG83iBt1DNTerR04YGaQ
    """

    if self.debug: 
      print(f"authenticator_login_type: {request.args.get('authenticator_login_type')}")
      print(f"client_id: {request.args.get('client_id')}")
      print(f"apiVer: {request.args.get('apiVer')}")
      print(f"serverEnvironment: {request.args.get('serverEnvironment')}")
      print(f"redirect_uri: {request.args.get('redirect_uri')}")
      print(f"release_type: {request.args.get('release_type')}")
      print(f"response_type: {request.args.get('response_type')}")
      sig = request.args.get('sig')
      sig_parts = []
      if sig:
        try:
          sig_parts = sig.split(".")
          sig_parts[0] = base64.b64decode(sig_parts[0] + "=" * (len(sig_parts[0]) % 3))
          print(f"sig_parts: {sig_parts}")
        except Exception as e:
          print(f"sig: {sig}")

    # Is this the first stage of an authention request? The "verify" code stage.
    if request.args.get("authenticator_login_type") == "mobile_ea_account":
      
      # Decode the sig argument
      sig = request.args.get("sig")
      if sig:
        # Decompse the sig into its body and hash
        body, body_hash = sig.split(".")
        if not body:
          about(403)
      
      # Build a long lived token
      # This is taken from an actual session. Once de-base64'd, it looks like:
      #    AT0:2.0:3.0:86400:KpnQGZO2SIxD0xKZEL9pBauVdBJYRO0NYD2:47082:riqac
      #    ^            ^        ^                                 ^    ^--- ?
      #    |            |        |                                  ---- game id?
      #    |            |         --- actual token?
      #    |             ------ token life (in seconds)
      #     ------ header ("AuthToken0"?)
      self.lnglv_token = "QVQwOjIuMDozLjA6ODY0MDA6S3BuUUdaTzJTSXhEMHhLWkVMOXBCYXVWZEJKWVJPME5ZRDI6NDcwODI6cmlxYWM"
      
      # Build the response json
      return jsonify(
        {
          "code": "QUOgAOq3MMhJN3hQu3WMxVKvNbDKwgSQoVX3d0YS",
          "lnglv_token": self.lnglv_token
        }
      ) 
     
    # If we are getting an anonymous token, just return one...
    # TODO: programmatically generate this
    return jsonify({"code": "QUOgAP3pKNGNI71KH72KdKjMkkFE5sO1nWWi34qGAQ"})




  def connect_auth_new(self):
    """Handler for accounts.ea.com/connect/auth"""
    self.print_headers()
    self.print_args()

    # Check the authenticator_login_type for compliance
    auth_login_type = request.args.get("authenticator_login_type")
    if auth_login_type not in ["mobile_anonymous", "mobile_ea_account"]:
      self.log_debug("Invalid authenticator_login_type: {auth_login_type}")
      self.error_invalid_request("authenticator_login_type is invalid", 102108)

    # Validate and get parameters
    client_id = self.validate_client_id()
    self.validate_redirect_uri()

    # Validate response_type. Additional verification follows below
    response_type = request.args.get("response_type")
    if not response_type:
      self.log_debug("response_type is missing")
      self.error_invalid_request("response_type is missing", 101109)

    # Get the nonce if it exist (lnglv_token)
    nonce = request.args.get("nonce")
    
    # Validate the requested response_type types
    response_types = response_type.split(" ")
    for r_type in response_types:
      if r_type not in ["code", "lnglv_token"]:
        self.log_debug(f"Unknown response_type given: {r_type}")
        self.error_invalid_request("unsupported_request_type", 101112)

      # lnglv_token requires a nonce
      if r_type == "lnglv_token" and not nonce:
        self.log_debug(f"Missing nonce for response_type of lnglv_token")
        self.error_invalid_request("nonce is missing", 102020)
   
    # Validate sig or lnglv_token header exist
    sig = request.args.get("sig")
    lnglv_token = request.headers.get("lnglv_token")    # TODO: validate this exists for this call
    if not sig and not lnglv_token:
      self.log_debug("sig and lnglv_token are missing")
      self.error_invalid_request("sig is missing", 102105)

    if sig and len(sig.split(".")) != 2:
      self.log_debug("sig is invalid format")
      self.error_invalid_request("sig is missing", 102105)

    # Additional error condition: if the sig isn't a valid base64.base64 combo
    # or if the first base64 does not contain valid/correct json the result is
    # the same error code 102105


    # This handler's function varies depending on the type of request as
    # specified by the authenticator_login_type parameter.
    if auth_login_type == "mobile_anonymous": 
      # Request initial (anonymous) access code

      # decompose the sig value
      body, sighash = sig.split(".")
      sig_dict = json.loads(base64.b64decode(self.fix_pad(body)))
      
      # Do I really care about these values?
      request_uuid = sig_dict.get("as")
      request_ts = sig_dict.get("ts")          
      
      # TODO: should i check the sighash for validity at some point?

      # Build an access code/token
      access_code = self.auth_manager.new_authorization_code(auth_login_type)

      return jsonify({"code": access_code})           

    elif auth_login_type == "mobile_ea_account":
      # Request longlv token or another auth token 

      # Determine the type of response requested
      response_data: dict = {}
      for resp_type in response_types:
        if "code" in response_types:
          response_data["code"] = self.auth_manager.new_authorization_code(
                                                      auth_login_type
                                                    )
        if "lnglv_token" in response_types:
          # Decompose the sig field
          body, sig_hash = sig.split(".")
          sig_dict = json.loads(base64.b64decode(self.fix_pad(body)))

          email_address = sig_dict.get("email")
          if not email_address or "@" not in email_address: 
            self.log_debug(f"Missing/invalid email address in {sig_dict}")
            self.error_invalid_request("INVALID_CODE", 102128)
          
          username, _ = email_address.split("@")
  
          lnglv_token = self.auth_manager.new_lnglv_token(
                                              username,
                                              client_id,
                                              response_data["code"]
                                          )
          if not lnglv_token:
            self.log_debug(f"Unable to load data for {username}")
            self.error_invalid_request("INSUFFICIENT_INFO")

          # Apply the token to the response
          response_data["lnglv_token"] = lnglv_token
          
          # NOTE ---------------------------
          # The lnglv_token associates a username with the persona base ID
          # and the code links to the lnglv_token. the code is used by 
          # connect/token?add_authenticator to link the user's authenticator
          # with the anonymous authenticator
          # ---------------------------------


      return jsonify(response_data)       

    else:   # missing authenticator_login_type
      self.error_invalid_request("REQUIRE_CODE", 102121)



  def connect_token(self):
    """Handler for accounts.ea.com/connect/token"""
    self.print_headers()
    self.print_args()

    # TODO: properly generate the access and refresh tokens
    self.access_token = base64.b64encode(
      "AT0:2.0:3.0:720:SHTNCm5XH18l7HfL0nefD8zQvjsNxlZF3vT:59901:rhal5".encode("utf-8")
    ).decode("utf-8")

    refresh_token_body = base64.b64encode(
      "RT0:2.0:3.0:1439:9WrwBPXBO03aH9mhN83m8NHkrpH5gyVcQ:59901:rhal5".encode("utf-8")
    ).decode("utf-8")
    # stealing a signature for now. TODO: calculate this
    refresh_token_sig = "MpDW6wVO8Ek79nu6jxMdSQwOqP"
    refresh_token = f"{refresh_token_body}.{refresh_token_sig}"

    id_token_header = base64.b64encode('{"typ":"JWT","alg":"HS256"}'.encode("utf-8")).decode("utf-8")
    id_token_body = base64.b64encode(
      json.dumps(
        { 
          "aud":"simpsons4-ios-client",
          "iss":"accounts.ea.com",
          "iat": int(round(time.time() * 1000)),
          "exp": int(round(time.time() * 1000)) + 3600,
          "pid_id": self.me_persona_pid_id,
          "user_id": self.token_user_id,
          "persona_id": self.token_persona_id,
          "pid_type":"AUTHENTICATOR_ANONYMOUS",
          "auth_time":0
        }
      ).encode("utf-8")
    ).decode("utf-8")
    # stealing a signature for now. TODO: calculate this
    id_token_sig = base64.b64encode(
      bytes.fromhex(
        "033b68a1deed4f9724690b1b69923bb719c56395128128dac76066713b1e"
      )
    ).decode("utf-8")
    id_token = f"{id_token_header}.{id_token_body}.{id_token_sig}"

    return jsonify(
      {
        "access_token": self.access_token,
        "token_type": "Bearer",
        "expires_in": 86400,     # 43199,
        "refresh_token": refresh_token,
        "refresh_token_expires_in": 86400, # 86399
        "id_token": id_token
      }
    )

  def connect_tokeninfo(self):
    """Handler for accounts.ea.com/connect/tokeninfo"""
    self.print_headers()
    self.print_args()

    access_token = request.headers.get('access_token')
    # If we don't have an access token, we are looking at an anonymous login
    if not access_token:
      return jsonify(
        {
            "client_id": "simpsons4-android-client",
            "scope": "offline basic.antelope.links.bulk openid signin antelope-rtm-readwrite search.identity basic.antelope basic.identity basic.persona antelope-inbox-readwrite",
            "expires_in": 86400,        # 43199
            "pid_id": self.me_persona_pid_id,
            "pid_type": "AUTHENTICATOR_ANONYMOUS",
            "user_id": self.token_user_id,
            "persona_id": self.token_persona_id,
            "authenticators": [
                {
                    "authenticator_type": "AUTHENTICATOR_ANONYMOUS",
                    "authenticator_pid_id": self.token_authenticator_pid_id
                }
            ],
            "is_underage": False,
            "stopProcess": "OFF",
            "telemetry_id": self.token_telemetry_id
        }
      )
    
    # If this is the long lived token, response with details about that token
    if access_token == self.lnglv_token:
      return jsonify(
        {
          "client_id": "long_live_token",
          "scope": "basic.identity basic.persona",
          "expires_in": 5184000,      # 60 days (in seconds))
          "pid_id": "1020010147082",
          "pid_type": "NUCLEUS",
          "user_id": "1020010147082",
          "persona_id": None
        }
      )
    
    # If this is the access_token, respond to that
    if access_token == self.access_token:
      return jsonify(
        {
          "client_id": "simpsons4-ios-client",
          "scope": "offline basic.antelope.links.bulk openid signin antelope-rtm-readwrite search.identity basic.antelope basic.identity basic.persona antelope-inbox-readwrite",
          "expires_in": 43199,
          "pid_id": self.me_persona_pid_id,
          "pid_type": "AUTHENTICATOR_ANONYMOUS",
          "user_id": self.token_user_id,
          "persona_id": self.token_persona_id,
          "authenticators": [
              {
                  "authenticator_type": "AUTHENTICATOR_ANONYMOUS",
                  "authenticator_pid_id": self.token_authenticator_pid_id
              },
              {
                  "authenticator_type": "NUCLEUS",
                  "authenticator_pid_id": self.token_authenticator_pid_id + 1
              }
          ],
          "is_underage": false,
          "stopProcess": "OFF",
          "telemetry_id": self.token_telemetry_id
        }
      )
    
    # otherwise, just error out
    abort(403)

  def geoagerequirements(self):
    """Handler for accounts.ea.com/proxy/identity/geoagerequirements"""
    self.print_headers()
    self.print_args()

    return jsonify(
      {
        "geoAgeRequirements": {
          "minLegalRegAge": 13,
          "minAgeWithConsent": "3",
          "minLegalContactAge": 13,
          "country": "US"             # TODO: make configurable
        }
      }
    )

  def probe(self):
    """Handler for accounts.ea.com/probe"""
    self.print_headers()
    self.print_args()
    return ""

  # server: pn.tnt-ea.com

  def game_devices(self, game_id: str):
    """Handler for pn.tnt-ea.com/games/<game_id>/devices and 
       pn.tnt-ea.com/rest/v1/games/<game_id>/devices"""

    self.print_headers()
    self.print_args()
    self.log_debug(f"game_id = {game_id}")

    # Expect game_id to be 48302, but doesn't matter if it changes
    req_json = request.get_json()
    if not req_json:
      abort(500)

    resp_json = {
      "deviceIdentifier": req_json.get("deviceIdentifier"),
      "userAlias": req_json.get("userAlias"),
      "appId": req_json.get("appId"),
      "deviceType": req_json.get("deviceType"),
      "operatingSystem": req_json.get("operatingSystem").lower(),
      "manufacturer": req_json.get("manufacturer"),
      "model": req_json.get("model"),
      "appVersion": req_json.get("appVersion"),
      "locale": req_json.get("locale"),
      "timezone": req_json.get("timezone"),
      "disabled": True if req_json.get("disabled") == "True" else False,
      "disableReason": req_json.get("disableReason"),
      "dateOfBirth": req_json.get("dateOfBirth"),
      "sandbox": True if req_json.get("") == "True" else False,  
      "silentIntervalStart": -1,
      "silentIntervalEnd": -1,
      "country": self.config.get("country"),
      "legacy": False
    }
    return jsonify(resp_json)

  # server river-mobile.data.ea.com

  def core_logEvent(self):
    """river-mobile.data.ea.com/tracking/api/core/logEvent"""
    self.print_headers()
    self.print_args()
    self.log_debug(f"Body: {request.data}")

    return jsonify({"status": "ok"})

  # server: pin-river.data.ea.com

  def pinEvents(self):
    """Handler for pin-river.data.ea.com/pinEvents"""
    self.print_headers()
    self.print_args()
    self.log_debug(request.get_json())   # get_json handles gzip decompression automatically

    resp = '{"status": "ok"}'
    response = make_response(resp)
    response.headers['Content-Type'] = 'application/json'
    return response
   
  # server: prod.simpsons-ea.com

  def lobby_time(self):
    
    now = self.now_int()
    now -= 365 * 86400 * 2
    now *= 1000
    old_method = int(round(time.time() * 1000))
    xml = f'<?xml version="1.0" encoding="UTF-8" standalone="yes"?><Time><epochMilliseconds>{now}</epochMilliseconds></Time>'
    response = make_response(xml)
    response.headers['Content-Type'] = 'application/xml'
    return response

  def tracking_log(self):
    """Handler for prod.simpsons-ea.com/mh/games/bg_gameserver_plugin/trackinglog/"""

    self.print_headers()
    self.print_args()

    req = ClientLog_pb2.ClientLogMessage()
    req.ParseFromString(request.data)
    
    self.log_debug(str(req))

    xml = f'<?xml version="1.0" encoding="UTF-8"?><Resources><URI>OK</URI></Resources>'
    response = make_response(xml)
    response.headers['Content-Type'] = 'application/xml'
    return response

  def protoClientConfig(self):
    """Handler for prod.simpsons-ea.com/mh/games/bg_gameserver_plugin/protoClientConfig/"""
    self.print_headers()
    self.print_args()

    client_config = ClientConfigData_pb2.ClientConfigResponse()

    for entry in self.config.get("protoClientConfig"):
      item = client_config.items.add()
      item.clientConfigId = entry[0]
      item.name = entry[1]
      item.value = entry[2]

    # Generate the response object
    response = make_response(client_config.SerializeToString())
    response.headers['Content-Type'] = 'application/x-protobuf'
    return response

  def gameplayconfig(self):
    """Handler for prod.simpsons-ea.com/mh/gameplayconfig"""

    # NOTE: if this returns an empty config, the app still loads.

    self.print_headers()
    self.print_args()

    client_config = GameplayConfigData_pb2.GameplayConfigResponse()

    for config_item in self.config.get("gameplayconfig"):
      item = client_config.item.add()
      item.name = config_item[0]
      item.value = config_item[1]

    response = make_response(client_config.SerializeToString())
    response.headers['Content-Type'] = 'application/x-protobuf'
    return response

  def mh_user(self):
    """Handler for prod.simpsons-ea.com/mh/users"""
    self.print_headers()
    self.print_args()

    data = AuthData_pb2.UsersResponseMessage()
    data.user.userId = self.user_user_id
    data.user.telemetryId = self.user_telemtry_id
    data.token.sessionKey = self.token_session_key
    
    response = make_response(data.SerializeToString())
    response.headers['Content-Type'] = 'application/x-protobuf'
    return response

  def userstats(self):
    """Handler for prod.simpsons-ea.com/mh/userstats"""
    self.print_headers()
    self.print_args()
    
    # This always returns 409 
    abort(409)

  def protoWholeLandToken(self, land_id):
    """Handler for prod.simpsons-ea.com/mh/games/bg_gameserver_plugin/protoWholeLandToken/<land_id>/"""
    self.print_headers()
    self.print_args()
    self.log_debug(f"land_id = {land_id}")

    req = WholeLandTokenData_pb2.WholeLandTokenRequest()
    req.ParseFromString(request.data)
    self.log_debug(str(req.requestId))

    data = WholeLandTokenData_pb2.WholeLandTokenResponse()
    data.token = self.land_token
    data.conflict = False
    
    response = make_response(data.SerializeToString())
    response.headers['Content-Type'] = 'application/x-protobuf'
    return response
 
  def checkToken(self, token):
    """Handler for prod.simpsons-ea.com/mh/games/bg_gameserver_plugin/checkToken/<token>/protoWholeLandToken/"""
    self.print_headers()
    self.print_args()
    self.log_debug(f"\ttoken: {token}")

    data = AuthData_pb2.TokenData()
    data.sessionKey = self.session_key
    data.expirationDate = 0
    
    response = make_response(data.SerializeToString())
    response.headers['Content-Type'] = 'application/x-protobuf'
    return response

  def trackingmetrics(self):
    """Handler for prod.simpsons-ea.com/mh/games/bg_gameserver_plugin/trackingmetrics/"""
    self.print_headers()
    self.print_args()
    
    req = ClientMetrics_pb2.ClientMetricsMessage()
    req.ParseFromString(request.data)
    self.log_debug(str(req))
   
    data = AuthData_pb2.TokenData()
    data.sessionKey = self.session_key
    data.expirationDate = 0
    
    xml = '<?xml version="1.0" encoding="UTF-8"?><Resources><URI>OK</URI></Resources>'
    response = make_response(xml)
    response.headers['Content-Type'] = 'application/xml'
    return response

  def bg_gameserver_plugin_protoland(self, land_id: str):
    """Handler prod.simpsons-ea.com/mh/games/bg_gameserver_plugin/protoland/<land_id>/"""
    self.print_headers()
    self.print_args()
    self.log_debug(f"land id: {land_id}")
      
    if request.method == 'GET':
      # Load town
      self.load_town()
      response = make_response(self.land_proto.SerializeToString())
      response.headers['Content-Type'] = 'application/x-protobuf'
      return response

    if request.method != "POST":
      abort(500)

    # Deserialize the current town state
    self.land_proto.ParseFromString(request.data)

    # Save town
    self.save_town()

    xml = '<?xml version="1.0" encoding="UTF-8" standalone="yes"?><WholeLandUpdateResponse/>'
    response = make_response(xml)
    response.headers['Content-Type'] = 'application/xml'
    return response
 
  def protocurrency(self, land_id: str):
    """Handler for prod.simpsons-ea.com/mh/games/bg_gameserver_plugin/protocurrency/<land_id>/"""
    self.print_headers()
    self.print_args()
    self.log_debug(f"land_id = {land_id}")

    currency = PurchaseData_pb2.CurrencyData()
    currency.id = land_id
    currency.vcTotalPurchased = 0
    currency.vcTotalAwarded = 0
    currency.vcBalance = 1234567                    # number of donuts
    currency.createdAt = int(round(time.time() * 1000))
    currency.updatedAt = int(round(time.time() * 1000))

    response = make_response(currency.SerializeToString())
    response.headers['Content-Type'] = 'application/x-protobuf'
    return response
 
  def extraLandUpdate(self, land_id: str):
    """Handler for prod.simpsons-ea.com/mh/games/bg_gameserver_plugin/extraLandUpdate/<land_id>/protoland/"""
    self.print_headers()
    self.print_args()

    # TODO: process the incoming update
    req = LandData_pb2.ExtraLandMessage()
    req.ParseFromString(request.data)

    self.log_debug(str(req))
    

    """
      if req.hasField("currencyDelta"): 
    
    message ExtraLandMessage {
      repeated .Data.ExtraLandMessage.CurrencyDelta currencyDelta = 1;
      repeated .Data.EventMessage event = 2;
      repeated .Data.ExtraLandMessage.PushNotification pushNotification = 3;
      repeated .Data.ExtraLandMessage.CommunityGoalDelta communityGoalDelta = 4;
      repeated .Data.ExtraLandMessage.MatchmakingRegistration matchmakingRegistration = 5;
      repeated .Data.PurchaseRequestMessage.DeviceIds deviceIds = 6;
      optional .Data.PurchaseRequestMessage.ApplicationInfo applicationInfo = 7;
      optional .Data.PurchaseRequestMessage.DeviceInfo deviceInfo = 8;
      message CurrencyDelta {
        optional int32 id = 1;
        optional string reason = 2;
        optional int32 amount = 3;
        optional int64 updatedAt = 4;
        optional string productId = 5;
      }
      message PushNotification {
        optional string id = 1;
        optional string toPlayerId = 2;
        optional int32 scheduledIn = 3;
        optional string templateName = 4;
        optional string message = 5;
      }
      message CommunityGoalDelta {
        optional string category = 1;
        optional int64 amount = 2;
      }
      message MatchmakingRegistration {
        optional string category = 1;
        repeated .Data.NameValue params = 2;
      }
    }
"""

    # TODO: figure out what the messages due
    extraland = LandData_pb2.ExtraLandResponse()
    response = make_response(extraland.SerializeToString())
    response.headers['Content-Type'] = 'application/x-protobuf'
    return response
    
    """
    repeated .Data.ExtraLandMessage.CurrencyDelta processedCurrencyDelta = 1;
    repeated .Data.EventMessage processedEvent = 2;
    repeated .Data.EventMessage receivedEvent = 3;
    repeated .Data.ExtraLandResponse.CommunityGoal communityGoal = 4;
    message CommunityGoal {
      optional string category = 1;
      optional int64 progressTotal = 2;
      optional double progressRate = 3;
      repeated int64 awardThresholds = 4;
    }
    """

  def plugin_event(self, land_id: str):
    """Handler for prod.simpsons-ea.com/mh/games/bg_gameserver_plugin/event/<land_id>/protoland/"""
    self.print_headers()
    self.print_args()
    self.log_debug(f"land_id = {land_id}")

    # TODO: figure out the use of this.
    event = LandData_pb2.EventsMessage()
    response = make_response(event.SerializeToString())
    response.headers['Content-Type'] = 'application/x-protobuf'
    return response


  # Generic default route handler

  def default_root(self):
    """Handler for default '/' routes"""
    self.print_headers()
    self.print_args()
    self.log_debug("/ called")

    return ""

  # Control panel route handlers
  def dashboard(self):
    """Handler for /dashboard"""

    return render_template("dashboard.html", play_modes=tsto_events.items(), current_play_mode = self.current_play_mode)

  def controller_update(self):
    """Handler for /controller/api/update"""
    play_mode_id = int(request.form.get('playmode', 0))
    self.log_debug(f"Switching game mode to : {tsto_events.get(play_mode_id, 'not found')}")
    self.set_game_mode(play_mode_id)
    return redirect("/dashboard")


def start_server():
  print("starting web server")
  server = TheSimpsonsTappedOutLocalServer()
  server.run()


if __name__ == "__main__":
  start_server()
