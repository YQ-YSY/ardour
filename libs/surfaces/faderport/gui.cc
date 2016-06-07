/*
    Copyright (C) 2015 Paul Davis

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#include <gtkmm/alignment.h>
#include <gtkmm/label.h>
#include <gtkmm/liststore.h>

#include "pbd/unwind.h"
#include "pbd/strsplit.h"
#include "pbd/file_utils.h"

#include "gtkmm2ext/bindings.h"
#include "gtkmm2ext/gtk_ui.h"
#include "gtkmm2ext/gui_thread.h"
#include "gtkmm2ext/utils.h"

#include "ardour/audioengine.h"
#include "ardour/filesystem_paths.h"

#include "faderport.h"
#include "gui.h"

#include "i18n.h"

using namespace PBD;
using namespace ARDOUR;
using namespace ArdourSurface;
using namespace std;
using namespace Gtk;
using namespace Gtkmm2ext;

void*
FaderPort::get_gui () const
{
	if (!gui) {
		const_cast<FaderPort*>(this)->build_gui ();
	}
	static_cast<Gtk::VBox*>(gui)->show_all();
	return gui;
}

void
FaderPort::tear_down_gui ()
{
	if (gui) {
		Gtk::Widget *w = static_cast<Gtk::VBox*>(gui)->get_parent();
		if (w) {
			w->hide();
			delete w;
		}
	}
	delete static_cast<FPGUI*> (gui);
	gui = 0;
}

void
FaderPort::build_gui ()
{
	gui = (void*) new FPGUI (*this);
}

/*--------------------*/

FPGUI::FPGUI (FaderPort& p)
	: fp (p)
	, table (2, 5)
	, action_table (5, 4)
	, ignore_active_change (false)
{
	set_border_width (12);

	table.set_row_spacings (4);
	table.set_col_spacings (6);
	table.set_border_width (12);
	table.set_homogeneous (false);

	std::string data_file_path;
	string name = "faderport-small.png";
	Searchpath spath(ARDOUR::ardour_data_search_path());
	spath.add_subdirectory_to_paths ("icons");
	find_file (spath, name, data_file_path);
	if (!data_file_path.empty()) {
		image.set (data_file_path);
		hpacker.pack_start (image, false, false);
	}

	Gtk::Label* l;
	Gtk::Alignment* align;
	int row = 0;

	input_combo.pack_start (midi_port_columns.short_name);
	output_combo.pack_start (midi_port_columns.short_name);

	input_combo.signal_changed().connect (sigc::bind (sigc::mem_fun (*this, &FPGUI::active_port_changed), &input_combo, true));
	output_combo.signal_changed().connect (sigc::bind (sigc::mem_fun (*this, &FPGUI::active_port_changed), &output_combo, false));

	l = manage (new Gtk::Label);
	l->set_markup (string_compose ("<span weight=\"bold\">%1</span>", _("Incoming MIDI on:")));
	l->set_alignment (1.0, 0.5);
	table.attach (*l, 0, 1, row, row+1, AttachOptions(FILL|EXPAND), AttachOptions(0));
	table.attach (input_combo, 1, 2, row, row+1, AttachOptions(FILL|EXPAND), AttachOptions(0), 0, 0);
	row++;

	l = manage (new Gtk::Label);
	l->set_markup (string_compose ("<span weight=\"bold\">%1</span>", _("Outgoing MIDI on:")));
	l->set_alignment (1.0, 0.5);
	table.attach (*l, 0, 1, row, row+1, AttachOptions(FILL|EXPAND), AttachOptions(0));
	table.attach (output_combo, 1, 2, row, row+1, AttachOptions(FILL|EXPAND), AttachOptions(0), 0, 0);
	row++;

	build_mix_action_combo (mix_combo[0], FaderPort::ButtonState(0));
	build_mix_action_combo (mix_combo[1], FaderPort::ShiftDown);
	build_mix_action_combo (mix_combo[2], FaderPort::LongPress);

	build_proj_action_combo (proj_combo[0], FaderPort::ButtonState(0));
	build_proj_action_combo (proj_combo[1], FaderPort::ShiftDown);
	build_proj_action_combo (proj_combo[2], FaderPort::LongPress);

	build_trns_action_combo (trns_combo[0], FaderPort::ButtonState(0));
	build_trns_action_combo (trns_combo[1], FaderPort::ShiftDown);
	build_trns_action_combo (trns_combo[2], FaderPort::LongPress);

	build_available_action_menu ();

	build_foot_action_combo (foot_combo[0], FaderPort::ButtonState(0));
	build_foot_action_combo (foot_combo[1], FaderPort::ShiftDown);
	build_foot_action_combo (foot_combo[2], FaderPort::LongPress);

	/* No shift-press combo for User because that is labelled as "next"
	 * (marker)
	 */

	build_user_action_combo (user_combo[0], FaderPort::ButtonState(0));
	build_user_action_combo (user_combo[1], FaderPort::LongPress);

	action_table.set_row_spacings (4);
	action_table.set_col_spacings (6);
	action_table.set_border_width (12);
	action_table.set_homogeneous (false);

	int action_row = 0;

	l = manage (new Gtk::Label);
	l->set_markup (string_compose ("<span weight=\"bold\">%1</span>", _("Press Action")));
	l->set_alignment (0.5, 0.5);
	action_table.attach (*l, 1, 2, action_row, action_row+1, AttachOptions(FILL|EXPAND), AttachOptions (0));
	l = manage (new Gtk::Label);
	l->set_markup (string_compose ("<span weight=\"bold\">%1</span>", _("Shift-Press Action")));
	l->set_alignment (0.5, 0.5);
	action_table.attach (*l, 2, 3, action_row, action_row+1, AttachOptions(FILL|EXPAND), AttachOptions (0));
	l = manage (new Gtk::Label);
	l->set_markup (string_compose ("<span weight=\"bold\">%1</span>", _("Long Press Action")));
	l->set_alignment (0.5, 0.5);
	action_table.attach (*l, 3, 4, action_row, action_row+1, AttachOptions(FILL|EXPAND), AttachOptions (0));
	action_row++;

	l = manage (new Gtk::Label);
	l->set_markup (string_compose ("<span weight=\"bold\">%1</span>", _("Mix")));
	l->set_alignment (1.0, 0.5);
	action_table.attach (*l, 0, 1, action_row, action_row+1, AttachOptions(FILL|EXPAND), AttachOptions (0));
	align = manage (new Alignment);
	align->set (0.0, 0.5);
	align->add (mix_combo[0]);
	action_table.attach (*align, 1, 2, action_row, action_row+1, AttachOptions(FILL|EXPAND), AttachOptions (0));
	align = manage (new Alignment);
	align->set (0.0, 0.5);
	align->add (mix_combo[1]);
	action_table.attach (*align, 2, 3, action_row, action_row+1, AttachOptions(FILL|EXPAND), AttachOptions (0));
	align = manage (new Alignment);
	align->set (0.0, 0.5);
	align->add (mix_combo[2]);
	action_table.attach (*align, 3, 4, action_row, action_row+1, AttachOptions(FILL|EXPAND), AttachOptions (0));
	action_row++;

	l = manage (new Gtk::Label);
	l->set_markup (string_compose ("<span weight=\"bold\">%1</span>", _("Proj")));
	l->set_alignment (1.0, 0.5);
	action_table.attach (*l, 0, 1, action_row, action_row+1, AttachOptions(FILL|EXPAND), AttachOptions (0));
	align = manage (new Alignment);
	align->set (0.0, 0.5);
	align->add (proj_combo[0]);
	action_table.attach (*align, 1, 2, action_row, action_row+1, AttachOptions(FILL|EXPAND), AttachOptions (0));
	align = manage (new Alignment);
	align->set (0.0, 0.5);
	align->add (proj_combo[1]);
	action_table.attach (*align, 2, 3, action_row, action_row+1, AttachOptions(FILL|EXPAND), AttachOptions (0));
	align = manage (new Alignment);
	align->set (0.0, 0.5);
	align->add (proj_combo[2]);
	action_table.attach (*align, 3, 4, action_row, action_row+1, AttachOptions(FILL|EXPAND), AttachOptions (0));
	action_row++;

	l = manage (new Gtk::Label);
	l->set_markup (string_compose ("<span weight=\"bold\">%1</span>", _("Trns")));
	l->set_alignment (1.0, 0.5);
	action_table.attach (*l, 0, 1, action_row, action_row+1, AttachOptions(FILL|EXPAND), AttachOptions (0));
	align = manage (new Alignment);
	align->set (0.0, 0.5);
	align->add (trns_combo[0]);
	action_table.attach (*align, 1, 2, action_row, action_row+1, AttachOptions(FILL|EXPAND), AttachOptions (0));
	align = manage (new Alignment);
	align->set (0.0, 0.5);
	align->add (trns_combo[1]);
	action_table.attach (*align, 2, 3, action_row, action_row+1, AttachOptions(FILL|EXPAND), AttachOptions (0));
	align = manage (new Alignment);
	align->set (0.0, 0.5);
	align->add (trns_combo[2]);
	action_table.attach (*align, 3, 4, action_row, action_row+1, AttachOptions(FILL|EXPAND), AttachOptions (0));
	action_row++;

	l = manage (new Gtk::Label);
	l->set_markup (string_compose ("<span weight=\"bold\">%1</span>", _("User")));
	l->set_alignment (1.0, 0.5);
	action_table.attach (*l, 0, 1, action_row, action_row+1, AttachOptions(FILL|EXPAND), AttachOptions (0));
	align = manage (new Alignment);
	align->set (0.0, 0.5);
	align->add (user_combo[0]);
	action_table.attach (*align, 1, 2, action_row, action_row+1, AttachOptions(FILL|EXPAND), AttachOptions (0));
	/* skip shift press combo */
	align = manage (new Alignment);
	align->set (0.0, 0.5);
	align->add (user_combo[1]);
	action_table.attach (*align, 3, 4, action_row, action_row+1, AttachOptions(FILL|EXPAND), AttachOptions (0));
	action_row++;

	l = manage (new Gtk::Label);
	l->set_markup (string_compose ("<span weight=\"bold\">%1</span>", _("Footswitch")));
	l->set_alignment (1.0, 0.5);
	action_table.attach (*l, 0, 1, action_row, action_row+1, AttachOptions(FILL|EXPAND), AttachOptions (0));
	align = manage (new Alignment);
	align->set (0.0, 0.5);
	align->add (foot_combo[0]);
	action_table.attach (*align, 1, 2, action_row, action_row+1, AttachOptions(FILL|EXPAND), AttachOptions (0));
	align = manage (new Alignment);
	align->set (0.0, 0.5);
	align->add (foot_combo[1]);
	action_table.attach (*align, 2, 3, action_row, action_row+1, AttachOptions(FILL|EXPAND), AttachOptions (0));
	align = manage (new Alignment);
	align->set (0.0, 0.5);
	align->add (foot_combo[2]);
	action_table.attach (*align, 3, 4, action_row, action_row+1, AttachOptions(FILL|EXPAND), AttachOptions (0));
	action_row++;

	table.attach (action_table, 0, 5, row, row+1, AttachOptions(FILL|EXPAND), AttachOptions (0));
	row++;

	hpacker.pack_start (table, true, true);
	pack_start (hpacker, false, false);

	/* update the port connection combos */

	update_port_combos ();

	/* catch future changes to connection state */

	fp.ConnectionChange.connect (connection_change_connection, invalidator (*this), boost::bind (&FPGUI::connection_handler, this), gui_context());
}

FPGUI::~FPGUI ()
{
}

void
FPGUI::connection_handler ()
{
	/* ignore all changes to combobox active strings here, because we're
	   updating them to match a new ("external") reality - we were called
	   because port connections have changed.
	*/

	PBD::Unwinder<bool> ici (ignore_active_change, true);

	update_port_combos ();
}

void
FPGUI::update_port_combos ()
{
	vector<string> midi_inputs;
	vector<string> midi_outputs;

	ARDOUR::AudioEngine::instance()->get_ports ("", ARDOUR::DataType::MIDI, ARDOUR::PortFlags (ARDOUR::IsOutput|ARDOUR::IsTerminal), midi_inputs);
	ARDOUR::AudioEngine::instance()->get_ports ("", ARDOUR::DataType::MIDI, ARDOUR::PortFlags (ARDOUR::IsInput|ARDOUR::IsTerminal), midi_outputs);

	Glib::RefPtr<Gtk::ListStore> input = build_midi_port_list (midi_inputs, true);
	Glib::RefPtr<Gtk::ListStore> output = build_midi_port_list (midi_outputs, false);
	bool input_found = false;
	bool output_found = false;
	int n;

	input_combo.set_model (input);
	output_combo.set_model (output);

	Gtk::TreeModel::Children children = input->children();
	Gtk::TreeModel::Children::iterator i;
	i = children.begin();
	++i; /* skip "Disconnected" */


	for (n = 1;  i != children.end(); ++i, ++n) {
		string port_name = (*i)[midi_port_columns.full_name];
		if (fp.input_port()->connected_to (port_name)) {
			input_combo.set_active (n);
			input_found = true;
			break;
		}
	}

	if (!input_found) {
		input_combo.set_active (0); /* disconnected */
	}

	children = output->children();
	i = children.begin();
	++i; /* skip "Disconnected" */

	for (n = 1;  i != children.end(); ++i, ++n) {
		string port_name = (*i)[midi_port_columns.full_name];
		if (fp.output_port()->connected_to (port_name)) {
			output_combo.set_active (n);
			output_found = true;
			break;
		}
	}

	if (!output_found) {
		output_combo.set_active (0); /* disconnected */
	}
}

void
FPGUI::build_available_action_menu ()
{
	/* build a model of all available actions (needs to be tree structured
	 * more)
	 */

	available_action_model = TreeStore::create (action_columns);

	vector<string> paths;
	vector<string> labels;
	vector<string> tooltips;
	vector<string> keys;
	vector<Glib::RefPtr<Gtk::Action> > actions;

	Gtkmm2ext::ActionMap::get_all_actions (paths, labels, tooltips, keys, actions);

	typedef std::map<string,TreeIter> NodeMap;
	NodeMap nodes;
	NodeMap::iterator r;


	vector<string>::iterator k;
	vector<string>::iterator p;
	vector<string>::iterator t;
	vector<string>::iterator l;

	available_action_model->clear ();

	TreeIter rowp;
	TreeModel::Row parent;

	/* Disabled item (row 0) */

	rowp = available_action_model->append();
	parent = *(rowp);
	parent[action_columns.name] = _("Disabled");

	/* Key aliasing */

	rowp = available_action_model->append();
	parent = *(rowp);
	parent[action_columns.name] = _("Shift");
	rowp = available_action_model->append();
	parent = *(rowp);
	parent[action_columns.name] = _("Control");
	rowp = available_action_model->append();
	parent = *(rowp);
	parent[action_columns.name] = _("Option");
	rowp = available_action_model->append();
	parent = *(rowp);
	parent[action_columns.name] = _("CmdAlt");


	for (l = labels.begin(), k = keys.begin(), p = paths.begin(), t = tooltips.begin(); l != labels.end(); ++k, ++p, ++t, ++l) {

		TreeModel::Row row;
		vector<string> parts;

		parts.clear ();

		split (*p, parts, '/');

		if (parts.empty()) {
			continue;
		}

		//kinda kludgy way to avoid displaying menu items as mappable
		if ( parts[1] == _("Main_menu") )
			continue;
		if ( parts[1] == _("JACK") )
			continue;
		if ( parts[1] == _("redirectmenu") )
			continue;
		if ( parts[1] == _("Editor_menus") )
			continue;
		if ( parts[1] == _("RegionList") )
			continue;
		if ( parts[1] == _("ProcessorMenu") )
			continue;

		if ((r = nodes.find (parts[1])) == nodes.end()) {

			/* top level is missing */

			TreeIter rowp;
			TreeModel::Row parent;
			rowp = available_action_model->append();
			nodes[parts[1]] = rowp;
			parent = *(rowp);
			parent[action_columns.name] = parts[1];

			row = *(available_action_model->append (parent.children()));

		} else {

			row = *(available_action_model->append ((*r->second)->children()));

		}

		/* add this action */

		if (l->empty ()) {
			row[action_columns.name] = *t;
			action_map[*t] = *p;
		} else {
			row[action_columns.name] = *l;
			action_map[*l] = *p;
		}

		string path = (*p);
		/* ControlProtocol::access_action() is not interested in the
		   legacy "<Actions>/" prefix part of a path.
		*/
		path = path.substr (strlen ("<Actions>/"));

		row[action_columns.path] = path;
	}
}

void
FPGUI::action_changed (Gtk::ComboBox* cb, FaderPort::ButtonID id, FaderPort::ButtonState bs)
{
	TreeModel::const_iterator row = cb->get_active ();
	string action_path = (*row)[action_columns.path];

	/* release binding */
	fp.set_action (id, action_path, false, bs);
}

void
FPGUI::build_action_combo (Gtk::ComboBox& cb, vector<pair<string,string> > const & actions, FaderPort::ButtonID id, FaderPort::ButtonState bs)
{
	Glib::RefPtr<Gtk::ListStore> model (Gtk::ListStore::create (action_columns));
	TreeIter rowp;
	TreeModel::Row row;
	string current_action = fp.get_action (id, false, bs); /* lookup release action */
	int active_row = -1;
	int n;
	vector<pair<string,string> >::const_iterator i;

	rowp = model->append();
	row = *(rowp);
	row[action_columns.name] = _("Disabled");
	row[action_columns.path] = string();

	if (current_action.empty()) {
		active_row = 0;
	}

	for (i = actions.begin(), n = 0; i != actions.end(); ++i, ++n) {
		rowp = model->append();
		row = *(rowp);
		row[action_columns.name] = i->first;
		row[action_columns.path] = i->second;
		if (current_action == i->second) {
			active_row = n+1;
		}
	}

	cb.set_model (model);
	cb.pack_start (action_columns.name);

	if (active_row >= 0) {
		cb.set_active (active_row);
	}

	cb.signal_changed().connect (sigc::bind (sigc::mem_fun (*this, &FPGUI::action_changed), &cb, id, bs));
}

void
FPGUI::build_mix_action_combo (Gtk::ComboBox& cb, FaderPort::ButtonState bs)
{
	vector<pair<string,string> > actions;

	actions.push_back (make_pair (string (_("Toggle Editor & Mixer Windows")), string (X_("Common/toggle-editor-and-mixer"))));
	actions.push_back (make_pair (string (_("Show/Hide Mixer list")), string (X_("Common/ToggleMixerList"))));
	actions.push_back (make_pair (string("Toggle Meterbridge"), string(X_("Common/toggle-meterbridge"))));
	actions.push_back (make_pair (string (_("Show/Hide Editor mixer strip")), string (X_("Editor/show-editor-mixer"))));

	build_action_combo (cb, actions, FaderPort::Mix, bs);
}

void
FPGUI::build_proj_action_combo (Gtk::ComboBox& cb, FaderPort::ButtonState bs)
{
	vector<pair<string,string> > actions;

	actions.push_back (make_pair (string("Toggle Editor Lists"), string(X_("Editor/show-editor-list"))));
	actions.push_back (make_pair (string("Toggle Summary"), string(X_("Editor/ToggleSummary"))));
	actions.push_back (make_pair (string("Toggle Meterbridge"), string(X_("Common/toggle-meterbridge"))));
	actions.push_back (make_pair (string (_("Zoom to Session")), string (X_("Editor/zoom-to-session"))));

//	actions.push_back (make_pair (string (_("Zoom In")), string (X_("Editor/temporal-zoom-in"))));
//	actions.push_back (make_pair (string (_("Zoom Out")), string (X_("Editor/temporal-zoom-out"))));

	build_action_combo (cb, actions, FaderPort::Proj, bs);
}

void
FPGUI::build_trns_action_combo (Gtk::ComboBox& cb, FaderPort::ButtonState bs)
{
	vector<pair<string,string> > actions;

	actions.push_back (make_pair (string("Toggle Big Clock"), string(X_("Window/toggle-big-clock"))));  //note:  this would really make sense if the Big Clock had transport buttons on it
	actions.push_back (make_pair (string("Toggle Locations window"), string(X_("Window/toggle-locations"))));
	actions.push_back (make_pair (string("Toggle Metronome"), string(X_("Transport/ToggleClick"))));
	actions.push_back (make_pair (string("Toggle External Sync"), string(X_("Transport/ToggleExternalSync"))));
	actions.push_back (make_pair (string("Toggle Follow Playhead"), string(X_("Editor/toggle-follow-playhead"))));

//	actions.push_back (make_pair (string("Set Playhead @pointer"), string(X_("Editor/set-playhead"))));


	build_action_combo (cb, actions, FaderPort::Trns, bs);
}

void
FPGUI::build_foot_action_combo (Gtk::ComboBox& cb, FaderPort::ButtonState bs)
{
	vector<pair<string,string> > actions;

	actions.push_back (make_pair (string("Toggle Roll"), string(X_("Transport/ToggleRoll"))));
	actions.push_back (make_pair (string("Toggle Rec-Enable"), string(X_("Transport/Record"))));
	actions.push_back (make_pair (string("Toggle Roll+Rec"), string(X_("Transport/record-roll"))));
	actions.push_back (make_pair (string("Toggle Loop"), string(X_("Transport/Loop"))));
	actions.push_back (make_pair (string("Toggle Click"), string(X_("Transport/ToggleClick"))));

	build_action_combo (cb, actions, FaderPort::Footswitch, bs);
}

bool
FPGUI::find_action_in_model (const TreeModel::iterator& iter, std::string const & action_path, TreeModel::iterator* found)
{
	TreeModel::Row row = *iter;
	string path = row[action_columns.path];

	if (path == action_path) {
		*found = iter;
		return true;
	}

	return false;
}

void
FPGUI::build_user_action_combo (Gtk::ComboBox& cb, FaderPort::ButtonState bs)
{
	cb.set_model (available_action_model);
	cb.pack_start (action_columns.name);
	cb.signal_changed().connect (sigc::bind (sigc::mem_fun (*this, &FPGUI::action_changed), &cb, FaderPort::User, bs));

	/* set the active "row" to the right value for the current button binding */

	string current_action = fp.get_action (FaderPort::User, false, bs); /* lookup release action */

	if (current_action.empty()) {
		cb.set_active (0); /* "disabled" */
		return;
	}

	TreeModel::iterator iter = available_action_model->children().end();

	available_action_model->foreach_iter (sigc::bind (sigc::mem_fun (*this, &FPGUI::find_action_in_model), current_action, &iter));

	if (iter != available_action_model->children().end()) {
		cb.set_active (iter);
	} else {
		cb.set_active (0);
	}

}

Glib::RefPtr<Gtk::ListStore>
FPGUI::build_midi_port_list (vector<string> const & ports, bool for_input)
{
	Glib::RefPtr<Gtk::ListStore> store = ListStore::create (midi_port_columns);
	TreeModel::Row row;

	row = *store->append ();
	row[midi_port_columns.full_name] = string();
	row[midi_port_columns.short_name] = _("Disconnected");

	for (vector<string>::const_iterator p = ports.begin(); p != ports.end(); ++p) {
		row = *store->append ();
		row[midi_port_columns.full_name] = *p;
		std::string pn = ARDOUR::AudioEngine::instance()->get_pretty_name_by_name (*p);
		if (pn.empty ()) {
			pn = (*p).substr ((*p).find (':') + 1);
		}
		row[midi_port_columns.short_name] = pn;
	}

	return store;
}

void
FPGUI::active_port_changed (Gtk::ComboBox* combo, bool for_input)
{
	if (ignore_active_change) {
		return;
	}

	TreeModel::iterator active = combo->get_active ();
	string new_port = (*active)[midi_port_columns.full_name];

	if (new_port.empty()) {
		if (for_input) {
			fp.input_port()->disconnect_all ();
		} else {
			fp.output_port()->disconnect_all ();
		}

		return;
	}

	if (for_input) {
		if (!fp.input_port()->connected_to (new_port)) {
			fp.input_port()->disconnect_all ();
			fp.input_port()->connect (new_port);
		}
	} else {
		if (!fp.output_port()->connected_to (new_port)) {
			fp.output_port()->disconnect_all ();
			fp.output_port()->connect (new_port);
		}
	}
}
