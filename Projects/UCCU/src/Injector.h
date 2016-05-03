#pragma once

#include <IQt5Wrapper.h>

class Injector {
public:
	static Injector& instance() { return *(ins ? ins : ins = new Injector()); }
private:
	Injector() {};
private:
	static Injector* ins;
public:
	bool Init(IQt5Wrpaaer* wrapper);
	bool OnExit();
	bool bQappTriggered;
	IQt5Wrpaaer* Wrapper;
};