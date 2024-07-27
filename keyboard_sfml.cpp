#include <SFML/Graphics.hpp>
#include <SFML/System.hpp> // sf::Clock ve sf::Time için
#include <SFML/Window/WindowStyle.hpp>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <cctype>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>

std::string x11_text;
std::mutex text_mutex;
bool textUpdated = false;

void x11_keyboard() {
  Display *display = XOpenDisplay(nullptr);
  if (!display) {
    std::cerr << "Unable to open X display" << std::endl;
    return;
  }

  Window root = DefaultRootWindow(display);
  Window curFocus;
  char buf[17];
  KeySym ks;
  XComposeStatus comp;
  int len;
  int revert;

  XGetInputFocus(display, &curFocus, &revert);
  XSelectInput(display, curFocus,
               KeyPressMask | KeyReleaseMask | FocusChangeMask);

  while (true) {
    XEvent ev;
    XNextEvent(display, &ev);
    switch (ev.type) {
    case FocusOut:
      if (curFocus != root) {
        XSelectInput(display, curFocus, 0);
      }
      XGetInputFocus(display, &curFocus, &revert);
      if (curFocus == PointerRoot) {
        curFocus = root;
      }
      XSelectInput(display, curFocus,
                   KeyPressMask | KeyReleaseMask | FocusChangeMask);
      break;

    case KeyPress:
      len = XLookupString(&ev.xkey, buf, 16, &ks, &comp);
      if (len > 0 && std::isprint(buf[0])) {
        buf[len] = '\0';
        std::lock_guard<std::mutex> lock(text_mutex);
        x11_text = buf;
        textUpdated = true;

      } else if (ks == XK_Escape) {
        // ESC key was pressed, handle accordingly
        std::cout << "ESC Key Pressed" << std::endl;
        std::lock_guard<std::mutex> lock(text_mutex);
        x11_text = "ESC";
        textUpdated = true;
      }
      break;
    }
  }

  XCloseDisplay(display);
}

void sfml_thread() {
  const int screenWidth = 1920;
  const int screenHeight = 1040;
  const int windowWidth = 200;
  const int windowHeight = 100;

  sf::RenderWindow window(sf::VideoMode(windowWidth, windowHeight), "", 0);
  window.setPosition(sf::Vector2i((screenWidth - windowWidth) / 2,
                                  screenHeight - windowHeight));
  window.setFramerateLimit(60);
  window.clear(sf::Color(0, 0, 0, 0));
  sf::Font font;
  if (!font.loadFromFile("./Arial.ttf")) {
    std::cerr << "Error loading font" << std::endl;
    return;
  }

  sf::Text text("", font, 50);
  text.setFillColor(sf::Color::White);
  sf::FloatRect textRect = text.getLocalBounds();
  text.setOrigin(textRect.width / 2, textRect.height / 2);
  text.setPosition(window.getSize().x / 2, window.getSize().y / 2);

  sf::Clock clock;
  bool isVisible = true;
  float displayDuration = 1.5f; // 500 ms
  float hiddenDuration = 0.5f;  // 500 ms

  while (window.isOpen()) {

    sf::Time elapsed = clock.getElapsedTime();
    float elapsedSeconds = elapsed.asSeconds();

    {
      std::lock_guard<std::mutex> lock(text_mutex);
      if (textUpdated) {
        text.setString(x11_text);
        textUpdated = false;
        isVisible = true;
        clock.restart();
        window.setVisible(true);
        elapsedSeconds = 0.0f;
      }
    }

    if (isVisible) {
      if (elapsedSeconds >= displayDuration) {
        window.setVisible(false);
        isVisible = false;
      }
    }
    if (isVisible) {
      sf::Event event;
      while (window.pollEvent(event)) {
        if (event.type == sf::Event::Closed) {
          window.close();
        }
      }

      if (window.isOpen()) {
        window.clear(sf::Color(0, 0, 0, 0));
        window.draw(text);
        window.display();
      }
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }
}

int main() {
  std::thread x11_thread(x11_keyboard);
  std::thread sfml_thread_instance(sfml_thread);

  x11_thread.join();
  sfml_thread_instance.join();

  return 0;
}
