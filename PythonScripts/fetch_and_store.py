import requests
import mysql.connector
from mysql.connector import Error
import time

# ThingSpeak API details
THINGSPEAK_URL = "https://api.thingspeak.com/channels/2765731/feeds.json"
READ_API_KEY = "6EX2CF6WPBY3NIUE"  # Replace with your ThingSpeak Read API key
RESULTS = 1  # Fetch the latest record

# MySQL database details
DB_HOST = "localhost"  # Replace with your MySQL server IP
DB_USER = "esp_user"       # MySQL username
DB_PASSWORD = "!55Oksen6792"  # MySQL password
DB_NAME = "esp_data"       # Database name


def fetch_data_from_thingspeak():
    """Fetch the latest data from ThingSpeak."""
    url = f"{THINGSPEAK_URL}?api_key={READ_API_KEY}&results={RESULTS}"
    try:
        response = requests.get(url)
        response.raise_for_status()  # Raise an error for bad status codes
        data = response.json()

        # Extract temperature and humidity
        feeds = data.get("feeds", [])
        if feeds:
            latest_entry = feeds[0]
            temperature = float(latest_entry.get("field1", 0))
            humidity = float(latest_entry.get("field2", 0))
            print(f"Fetched Data - Temperature: {temperature}, Humidity: {humidity}")
            return temperature, humidity
        else:
            print("No data available in ThingSpeak.")
            return None, None
    except requests.exceptions.RequestException as e:
        print(f"Error fetching data from ThingSpeak: {e}")
        return None, None


def store_data_in_mysql(temperature, humidity):
    """Store the fetched data into MySQL database."""
    try:
        connection = mysql.connector.connect(
            host=DB_HOST,
            user=DB_USER,
            password=DB_PASSWORD,
            database=DB_NAME
        )

        if connection.is_connected():
            cursor = connection.cursor()
            query = "INSERT INTO sensor_readings (temperature, humidity) VALUES (%s, %s)"
            cursor.execute(query, (temperature, humidity))
            connection.commit()
            print("Data stored successfully in MySQL.")

    except Error as e:
        print(f"Error connecting to MySQL: {e}")
    finally:
        if connection.is_connected():
            cursor.close()
            connection.close()


def main():
    while True:
        temperature, humidity = fetch_data_from_thingspeak()
        if temperature is not None and humidity is not None:
            store_data_in_mysql(temperature, humidity)
        else:
            print("Skipping database storage due to invalid data.")
        time.sleep(30)  # Wait for 30 seconds before fetching data again


if __name__ == "__main__":
    main()
