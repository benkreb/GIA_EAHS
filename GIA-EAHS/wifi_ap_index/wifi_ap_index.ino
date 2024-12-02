#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiAP.h>
#include <SPIFFS.h>

#ifndef LED_BUILTIN
#define LED_BUILTIN 2
#endif

// Bu kod, bir ESP32 cihazını Wi-Fi erişim noktası (Access Point) olarak çalıştırır. 
// Cihaz, gelen POST ve GET isteklerini işleyerek, kullanıcı mesajlarını kaydeder, listeyi temizler veya son mesajı döner.
// Tüm mesajlar SPIFFS dosya sistemine kaydedilir ve istekler arasında kalıcılığı sağlanır.

// **Yapılacaklar**
// 1. Mesajlarla birlikte eski index ve gönderen IP bilgisini yollama özelliği eklenmelidir.
// 2. POST isteğiyle alınan mesajlarda, mesaj içeriklerinin ve kullanıcı bilgilerinin doğru işlendiğinden emin olunmalıdır.
// 3. GET /latest isteği, yalnızca son mesajı göndermek için optimize edilmelidir (sadece mesaj içeriği yollanmalı).


const char *ssid = "EAHS_Routerr";       // Wi-Fi access point name
const char *password = "Furkan31";       // Wi-Fi password

WiFiServer server(80);                   // HTTP server
String messages = "";                    // Stores all messages
String latestMessage = "";               // Stores the most recent message
int messageIndex = 0;                    // Index for tracking messages
bool messagesLoaded = false;             // Tracks if messages have already been loaded

void setup() {
    pinMode(LED_BUILTIN, OUTPUT);

    Serial.begin(115200);
    Serial.println();
    Serial.println("Configuring access point...");

    // Initialize the access point
    if (!WiFi.softAP(ssid, password)) {
        log_e("Soft AP creation failed.");
        while (1);
    }

    IPAddress myIP = WiFi.softAPIP();
    Serial.print("AP IP address: ");
    Serial.println(myIP);
    server.begin(); // Start the HTTP server

    Serial.println("Server started");

    // Initialize the SPIFFS filesystem
    if (!SPIFFS.begin(true)) {
        Serial.println("An error has occurred while mounting SPIFFS");
        return;
    }

    loadMessages(); // Load messages from the filesystem
}

void loop() {
    WiFiClient client = server.available();

    if (client) {
        Serial.println("New Client.");
        String request = "";
        bool isPostRequest = false;
        int contentLength = 0;

        // Read HTTP requests from the client
        while (client.connected()) {
            if (client.available()) {
                char c = client.read();
                Serial.write(c);
                request += c;

                // Detect the end of the HTTP request
                if (request.endsWith("\r\n\r\n")) {
                    if (request.startsWith("POST")) {
                        isPostRequest = true;
                        contentLength = extractContentLength(request);
                        handlePostRequest(client, contentLength); // Handle POST request
                    } else if (request.startsWith("GET")) {
                        // Handle GET request
                        if (request.indexOf("/clear") > 0) {
                            handleClearRequest(client);
                        } else if (request.indexOf("/latest") > 0) {
                            handleLatestMessageRequest(client);
                        } else {
                            handleGetRequest(client, request);
                        }
                    }
                    break;
                }
            }
        }
        client.stop();
        Serial.println("Client Disconnected.");
    }
}

void handleGetRequest(WiFiClient &client, const String &request) {
    sendHttpResponse(client); // Send the HTML interface
}

void handlePostRequest(WiFiClient &client, int contentLength) {
    // Read the content of the POST request
    char postData[contentLength + 1];
    int bytesRead = client.readBytes(postData, contentLength);
    postData[bytesRead] = '\0';

    // Process the POST data
    String postString = String(postData);
    String name = extractValue(postString, "name");
    String messageContent = extractValue(postString, "message");

    // Decode the URL-encoded values
    name.replace('+', ' ');
    messageContent.replace('+', ' ');

    name = urlDecode(name);
    messageContent = urlDecode(messageContent);

    // If the data is valid, proceed
    if (name.length() > 0 && messageContent.length() > 0) {
        String clientIP = client.remoteIP().toString();
        String newMessage = String(messageIndex) + "," + clientIP + "," + name + "," + messageContent;

        // Prevent duplicate messages
        if (!messages.endsWith(newMessage + "<br>")) {
            latestMessage = messageContent;
            messages += newMessage + "<br>";
            messageIndex++;
            saveMessages(); // Save the message
        }
    }

    sendRedirect(client); // Redirect after POST to avoid duplication
}

void handleClearRequest(WiFiClient &client) {
    messages = ""; // Clear all messages
    messageIndex = 0;
    saveMessages();
    sendHttpResponse(client);
}

void handleLatestMessageRequest(WiFiClient &client) {
    // Send the most recent message as plain text
    client.println("HTTP/1.1 200 OK");
    client.println("Content-type:text/plain");
    client.println();
    client.println(latestMessage);
}

void sendHttpResponse(WiFiClient &client) {
    // Send the HTML interface as an HTTP response
    client.println("HTTP/1.1 200 OK");
    client.println("Content-type:text/html");
    client.println();

    client.println("<html><body>");
    client.println("<h2>Message Board</h2>");
    client.println("<form method='POST' action='/'>");
    client.println("Name: <input type='text' name='name'><br>");
    client.println("Message: <input type='text' name='message'><br>");
    client.println("<input type='submit' value='Send Message'>");
    client.println("</form>");
    client.println("<form method='GET' action='/clear'>");
    client.println("<button type='submit'>Clear Messages</button>");
    client.println("</form>");

    client.println("<h3>Messages:</h3>");
    client.println("<table border='1'>");
    client.println("<tr><th>Index</th><th>IP</th><th>Name</th><th>Message</th></tr>");
    if (messages.length() > 0) {
        String messageDisplay = messages;
        messageDisplay.replace("<br>", "</td></tr><tr><td>");
        messageDisplay.replace(",", "</td><td>");
        client.println("<tr><td>" + messageDisplay + "</td></tr>");
    }
    client.println("</table>");
    client.println("</body></html>");

    client.println();
}

void sendRedirect(WiFiClient &client) {
    client.println("HTTP/1.1 303 See Other");
    client.println("Location: /");
    client.println();
}

int extractContentLength(const String &request) {
    // Extract the Content-Length value from the HTTP header
    int index = request.indexOf("Content-Length:");
    if (index != -1) {
        int start = index + 15;
        int end = request.indexOf("\r\n", start);
        return request.substring(start, end).toInt();
    }
    return 0;
}

String extractValue(const String &data, const String &key) {
    // Extract the value associated with a specific key in the POST data
    int start = data.indexOf(key + "=") + key.length() + 1;
    int end = data.indexOf("&", start);
    if (end == -1) end = data.length();
    return data.substring(start, end);
}

String urlDecode(String input) {
    // Decode a URL-encoded string
    String decoded = "";
    char temp[] = "00";
    unsigned int i, j;
    for (i = 0; i < input.length(); i++) {
        if (input[i] == '+') {
            decoded += ' ';
        } else if (input[i] == '%') {
            temp[0] = input[i + 1];
            temp[1] = input[i + 2];
            decoded += (char)strtol(temp, NULL, 16);
            i += 2;
        } else {
            decoded += input[i];
        }
    }
    return decoded;
}

void saveMessages() {
    // Save messages to the SPIFFS filesystem
    File file = SPIFFS.open("/messages.txt", FILE_WRITE);
    if (!file) {
        Serial.println("Failed to open file for writing");
        return;
    }
    file.print(messages);
    file.close();
}

void loadMessages() {
    // Load messages from the SPIFFS filesystem
    if (messagesLoaded) {
        return; // Messages are already loaded
    }

    File file = SPIFFS.open("/messages.txt", FILE_READ);
    if (!file) {
        Serial.println("Failed to open file for reading");
        return;
    }

    while (file.available()) {
        messages += (char)file.read();
    }
    file.close();

    messagesLoaded = true; // Mark messages as loaded
}
