/*
    This file is a part of the RepSnapper project.
    Copyright (C) 2010 Michael Meeks

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#include <cstdio>
#include <cerrno>
#include <string>

#include <glib/gi18n.h>

#include "settings.h"
#include "connectview.h"
#include "model.h"

void ConnectView::serial_state_changed(SerialState state) {
  bool sensitive;
  const char *label;
  Gtk::BuiltinStockID id;

  switch (state) {
  case SERIAL_DISCONNECTING:
    id = Gtk::Stock::NO;
    label = _("Disconnecting...");
    sensitive = false;
    break;
  case SERIAL_DISCONNECTED:
    m_combo.set_sensitive (true);
    id = Gtk::Stock::NO;
    label = _("Connect");
    sensitive = true;
    break;
  case SERIAL_CONNECTING:
    m_combo.set_sensitive (false);
    id = Gtk::Stock::NO;
    label = _("Connecting...");
    sensitive = false;
    break;
  case SERIAL_CONNECTED:
  default:
    id = Gtk::Stock::YES;
    label = _("Disconnect");
    sensitive = true;
    break;
  }

  m_image.set (id, Gtk::ICON_SIZE_BUTTON);
  m_connect.set_label (label);
  m_connect.set_sensitive(sensitive);
  if (sensitive) {
    m_setting_state = true; // inhibit unhelpful recursion.
    m_connect.set_active (state == SERIAL_CONNECTED);
    m_setting_state = false;
  }
}

void ConnectView::connect_toggled() {
  if (!m_setting_state)
    m_printer->Connect(m_connect.get_active());
}

void ConnectView::signal_entry_changed() {
  // Use the value of the entry widget, rather than the
  // active text, so the user can enter other values.
  Gtk::Entry *entry = m_combo.get_entry();
  if (entry != NULL)
    m_settings->set_string("Hardware","PortName", entry->get_text());
}

void ConnectView::find_ports() {
  m_combo.remove_all();

  string port_setting = m_settings->get_string("Hardware","PortName");

  m_combo.append(port_setting);

  vector<string> ports = PrinterSerial::FindPorts();

  for(size_t i = 0; i < ports.size(); i++) {
    cout << "Found port: " << ports[i] << endl;
    if (ports[i] != port_setting) {
      m_combo.append(ports[i]);
    }
  }
}

ConnectView::ConnectView(Printer *printer,
			 Settings *settings,
			 bool show_connect)
  : Gtk::Box(Gtk::ORIENTATION_HORIZONTAL), m_connect(), m_port_label(_("Port:")),
  m_combo(true), m_settings(settings), m_printer(printer) {
  m_setting_state = false;

  set_spacing(2);
  add(m_image);
  add(m_connect);
  add(m_port_label);
  add(m_combo);

  m_connect.signal_toggled().connect(sigc::mem_fun(*this, &ConnectView::connect_toggled));
  m_combo.signal_changed().connect(sigc::mem_fun(*this, &ConnectView::signal_entry_changed));
  //m_combo.signal_popup_menu().connect(sigc::mem_fun(*this, &ConnectView::find_ports));

  show_all();
  if (!show_connect)
    m_connect.hide();
  serial_state_changed(SERIAL_DISCONNECTED);
  m_printer->signal_serial_state_changed.connect
    (sigc::mem_fun(*this, &ConnectView::serial_state_changed));

  // TODO: Execute find_ports every time the dropdown is displayed
  find_ports();
  m_combo.set_active(0);
}
