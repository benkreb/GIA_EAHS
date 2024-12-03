// Bu kod, bir Wi-Fi erişim noktası ve web sunucusu oluşturur. Kullanıcılar, basit bir web arayüzü aracılığıyla mesaj gönderip görüntüleyebilir.
// Gönderilen mesajlar SPIFFS üzerinde saklanır ve cihaz sıfırlansa veya sayfa yenilense bile mesajlar korunur.

// **Yapılacaklar**
// 1. Mesajları temizlemek için bir seçenek ekleyin.
// 2. Sayfa yenilendiğinde veya yeniden yüklendiğinde mesajların sürekli olarak görüntülenmesini sağlayın.
// 3. En son gönderilen mesajı ayrı bir HTTP uç noktası aracılığıyla alınmasını sağlayın.
// 4. Wi-Fi'den LoRa'ya dönüşüm ve optimizasyon işlemleri yapılacak.
// 5. Mesaj gönderimi sırasında index ve kullanıcı bilgileri eklenmiş durumda.

#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiAP.h>
#include <SPIFFS.h>

#ifndef LED_BUILTIN
#define LED_BUILTIN 2
#endif

const char *ssid = "EAHS_Routerr";
const char *password = "Furkan31";

WiFiServer server(80);
String messages = "";      // Stores all messages
String latestMessage = ""; // Stores the most recent message
int messageIndex = 0;      // Index for each message
bool messagesLoaded = false; // To track if messages are already loaded

void setup() {
    pinMode(LED_BUILTIN, OUTPUT);

    Serial.begin(115200);
    Serial.println();
    Serial.println("Configuring access point...");

    // Set up the soft access point (AP)
    if (!WiFi.softAP(ssid, password,1,0,10)) {
        log_e("Soft AP creation failed.");
        while (1); // Stop execution if AP creation fails
    }

    IPAddress myIP = WiFi.softAPIP(); // Get the IP address of the AP
    Serial.print("AP IP address: ");
    Serial.println(myIP);
    server.begin(); // Start the web server

    Serial.println("Server started");

    // Initialize SPIFFS (SPI Flash File System)
    if (!SPIFFS.begin(true)) {
        Serial.println("An error has occurred while mounting SPIFFS");
        return;
    }

    loadMessages(); // Load stored messages from SPIFFS
}

void loop() {
    WiFiClient client = server.available(); // Check for incoming client requests

    if (client) {
        Serial.println("New Client.");
        String request = ""; // Variable to store the HTTP request
        bool isPostRequest = false;
        int contentLength = 0;

        // Read the request from the client
        while (client.connected()) {
            if (client.available()) {
                char c = client.read();
                Serial.write(c);
                request += c;

                // If the request ends, handle the HTTP request
                if (request.endsWith("\r\n\r\n")) {
                    if (request.startsWith("POST")) {
                        isPostRequest = true;
                        contentLength = extractContentLength(request);
                        handlePostRequest(client, contentLength); // Handle POST request
                    } else if (request.startsWith("GET")) {
                        if (request.indexOf("/clear") > 0) {
                            handleClearRequest(client); // Clear messages
                        } else if (request.indexOf("/latest") > 0) {
                            handleLatestMessageRequest(client); // Fetch latest message
                        } else {
                            handleGetRequest(client, request); // Display message board
                        }
                    }
                    break;
                }
            }
        }
        client.stop(); // Close the connection after processing the request
        Serial.println("Client Disconnected.");
    }
}

void handleGetRequest(WiFiClient &client, const String &request) {
    sendHttpResponse(client); // Send the main page response
}

void handlePostRequest(WiFiClient &client, int contentLength) {
    char postData[contentLength + 1];
    int bytesRead = client.readBytes(postData, contentLength);
    postData[bytesRead] = '\0';

    String postString = String(postData);
    String name = extractValue(postString, "name"); // Extract name from form data
    String messageContent = extractValue(postString, "message"); // Extract message content

    name.replace('+', ' '); // Replace '+' with space in the name
    messageContent.replace('+', ' ');

    // Decode URL encoding for both name and message
    name = urlDecode(name);
    messageContent = urlDecode(messageContent);

    // If both name and message are not empty, store the message
    if (name.length() > 0 && messageContent.length() > 0) {
        String clientIP = client.remoteIP().toString(); // Get the client's IP address
        String newMessage = String(messageIndex) + "," + clientIP + "," + name + "," + messageContent;

        // Avoid duplicate messages
        if (!messages.endsWith(newMessage + "<br>")) {
            latestMessage = messageContent; // Update latest message
            messages += newMessage + "<br>"; // Add new message to the stored messages
            messageIndex++; // Increment message index
            saveMessages(); // Save the updated messages to SPIFFS
        }
    }

    // Redirect to the main page to avoid duplicate form submission
    sendRedirect(client);
}

void handleClearRequest(WiFiClient &client) {
    // Clear all stored messages
    messages = "";
    messageIndex = 0;
    saveMessages(); // Save the cleared messages
    sendHttpResponse(client); // Send the main page response
}

void handleLatestMessageRequest(WiFiClient &client) {
    // Send the latest message in plain text format
    client.println("HTTP/1.1 200 OK");
    client.println("Content-type:text/plain");
    client.println();
    client.println(latestMessage); // Send the latest message
}

void sendHttpResponse(WiFiClient &client) {
    // Send an HTML response to the client with the message board and form
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
    // Send HTTP redirect to the main page
    client.println("HTTP/1.1 303 See Other");
    client.println("Location: /");
    client.println();
}

int extractContentLength(const String &request) {
    // Extract content length from the HTTP request
    int index = request.indexOf("Content-Length:");
    if (index != -1) {
        int start = index + 15;
        int end = request.indexOf("\r\n", start);
        return request.substring(start, end).toInt();
    }
    return 0;
}

String extractValue(const String &data, const String &key) {
    // Extract a specific value from the form data
    int start = data.indexOf(key + "=") + key.length() + 1;
    int end = data.indexOf("&", start);
    if (end == -1) end = data.length();
    return data.substring(start, end);
}

String urlDecode(String input) {
    // URL decode the input string
    String decoded = "";
    char temp[] = "00";
    unsigned int i, j;
    for (i = 0; i < input.length(); i++) {
        if (input[i] == '+') {
            decoded += ' '; // Replace '+' with space
        } else if (input[i] == '%') {
            temp[0] = input[i + 1];
            temp[1] = input[i + 2];
            decoded += (char)strtol(temp, NULL, 16); // Convert %XX to character
            i += 2;
        } else {
            decoded += input[i]; // Append normal characters
        }
    }
    return decoded;
}

void saveMessages() {
    // Save messages to SPIFFS
    File file = SPIFFS.open("/messages.txt", FILE_WRITE);
    if (!file) {
        Serial.println("Failed to open file for writing");
        return;
    }
    file.print(messages); // Write messages to file
    file.close();
}

void loadMessages() {
    // Load messages from SPIFFS if they have not been loaded yet
    if (messagesLoaded) {
        return; // Skip if messages are already loaded
    }

    File file = SPIFFS.open("/messages.txt", FILE_READ);
    if (!file) {
        Serial.println("Failed to open file for reading");
        return;
    }

    while (file.available()) {
        messages += (char)file.read(); // Load stored messages
    }
    file.close();

    messagesLoaded = true; // Mark that messages are loaded
}
