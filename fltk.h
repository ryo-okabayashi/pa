#include <fltk/Window.h>
#include <fltk/Widget.h>
#include <fltk/Button.h>
#include <fltk/Slider.h>
#include <fltk/run.h>

using namespace fltk;

int exit_flg = 0;

void exit_callback(Widget *, void *) {
	exit(0);
}

void* thread1(void *args)
{
	Window *window = new Window(10, 10, 70, 40);
	window->begin();

	Button exit_button(10, 10, 50, 20, "exit.");
	exit_button.callback(exit_callback);

	window->end();
	window->show();
	run();

	exit_flg = 1;
}
