#pragma once


#include <memory>

namespace chrono {
    class ChSystem;
}



namespace hydroc {
    namespace gui {


struct UI {

	UI()
	{
	}
	virtual ~UI() {}


	UI(const UI&) = delete;
	UI& operator = (const UI&) = delete;

	/**@brief Initialize the system
	 * 
	 * Should be called after the given ChSystem is fully initialized
	 * The best is to call it just before the simulation loop that call IsRunning
	 * 
	*/
	virtual void Init(chrono::ChSystem*, const char* title);

	/**@brief To call during simulation loop  
	 * 
	*/
	virtual bool IsRunning(double timestep);


	bool simulationStarted = true;

protected:

	int frame = 0;

	chrono::ChSystem* pSystem = nullptr; // Do not manage the memory

};


class GUIImpl;


struct GUI: public UI {

	GUI();
	GUI(const GUI&) = delete;
	GUI& operator = (const GUI&) = delete;

	void Init(chrono::ChSystem*, const char* title) override;
	bool IsRunning(double timestep) override;

private:
	std::shared_ptr<hydroc::gui::GUIImpl> pImpl;
};


/**@brief Factory to create UI or GUI
 * 
*/
std::shared_ptr<hydroc::gui::UI> CreateUI(bool visualizationOn = true);


	} // end namespace gui
	} // end namespace hydroc





