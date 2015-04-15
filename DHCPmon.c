#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#include <pthread.h>

//Networking
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
//Notifications
#include <glib.h>
#include <libnotify/notify.h>

//Notification Icon and Log Window
#include <gtk/gtk.h>

#include "dhcp.h"

#define PORT 67
#define DHCPLENGTH 300 //Size of a DHCP message
#define HOSTNAMELEN 256
#define IPLEN 16
#define MACLEN 18
#define URGENCY 1 //This means that our notifications will apear before any other non-critical notification, and will also appear over fullscreen programs (which is what we actualy want...)

GtkTextBuffer* dhcplog;

static void showLogWindow(GtkMenuItem *item, gpointer arg){
	GtkWidget* window;
	GtkWidget* textbox;
	GtkWidget* vbox;
	GtkWidget* scrolled_window;
	

	//Set up the window
	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
	gtk_window_set_default_size(GTK_WINDOW(window), 750, 300);
	gtk_window_set_title(GTK_WINDOW(window), "DHCP Monitor log");
	gtk_container_set_border_width(GTK_CONTAINER(window), 5);
	GTK_WINDOW(window)->allow_shrink = FALSE;

	vbox = gtk_vbox_new(FALSE, 0);
	textbox = gtk_text_view_new_with_buffer(dhcplog);
	gtk_text_view_set_editable((GtkTextView*)textbox, FALSE);
	scrolled_window = gtk_scrolled_window_new(NULL, NULL);

  	gtk_container_add(GTK_CONTAINER(scrolled_window),textbox);
	gtk_container_add(GTK_CONTAINER(vbox), scrolled_window);
	gtk_container_add(GTK_CONTAINER(window), vbox);

	gtk_widget_show_all(GTK_WIDGET(window));
}

static void trayExit(GtkMenuItem *item, gpointer user_data){
    exit(0);
}

static void trayIconPopup(GtkStatusIcon *status_icon, guint button, guint32 activate_time, gpointer popUpMenu){
    gtk_menu_popup(GTK_MENU(popUpMenu), NULL, NULL, gtk_status_icon_position_menu, status_icon, button, activate_time);
}


void *gui_thread(void *arg) {
	GtkStatusIcon *status_icon;
	GdkPixbuf* pixbuf;
	GtkWidget *menu, *menuItemView, *menuItemExit;

	gtk_init(0, NULL);
	
	//Set up the tray icon
	//This is using a fixed path from ubuntu 12.10 !!!!!!! BAD IDEA!!!
	pixbuf = gdk_pixbuf_new_from_file("/usr/share/icons/notification-network-ethernet-connected.svg", NULL);

	status_icon = gtk_status_icon_new_from_pixbuf(pixbuf);
	gtk_status_icon_set_visible(status_icon, 1);

	menu = gtk_menu_new();
	menuItemView = gtk_menu_item_new_with_label ("View");
	menuItemExit = gtk_menu_item_new_with_label ("Exit");
	g_signal_connect (G_OBJECT (menuItemView), "activate", G_CALLBACK (showLogWindow), NULL);
	g_signal_connect (G_OBJECT (menuItemExit), "activate", G_CALLBACK (trayExit), NULL);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuItemView);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuItemExit);
	gtk_widget_show_all (menu);

	g_signal_connect(GTK_STATUS_ICON (status_icon), "activate", GTK_SIGNAL_FUNC (showLogWindow), NULL);
	g_signal_connect(GTK_STATUS_ICON (status_icon), "popup-menu", GTK_SIGNAL_FUNC (trayIconPopup), menu);


	gtk_main();
	

	return NULL; //This shouldnt even be executed, just here to avoid a warning.

}

void createNotification(const char* ip, const char* mac, const char* hname){
	NotifyNotification* notification;
	NotifyUrgency urgency;
	GError* error = NULL;
	char buffer[22+HOSTNAMELEN+IPLEN+MACLEN]; //22 is the number of characters in the message plus '\0'	
	if(!notify_init("DHCPMonitor")){
		printf("Error: could not initialize notifications.\n");
		return;
	}

	sprintf(buffer, "Hostname: %s\nIP: %s\nMAC: %s", hname, ip, mac);

	notification = notify_notification_new("Client connected:", buffer, "notification-network-ethernet-connected");//, NULL);
	if(URGENCY)
		urgency = NOTIFY_URGENCY_CRITICAL;
	else
		urgency = NOTIFY_URGENCY_NORMAL;
	notify_notification_set_urgency(notification, urgency);
	notify_notification_show(notification, &error);

	notify_uninit ();
}

int main(int argc, char** argv){
	int fd; //socket file descriptor
	unsigned char buffer[DHCPLENGTH];
	socklen_t addrlen;
	struct sockaddr_in serveraddr, clientaddr;
	int recvBytes, ptr, i,  yes = 1;
	struct dhcp_packet* dhcp;
	struct in_addr clientAddr;
	struct hostent* clientHName;
	unsigned int reqIP;
	char clientMAC[MACLEN];
	char clientIP[IPLEN];
	char clientHostname[HOSTNAMELEN];
	unsigned char messageType, requestedIP[4], length;
	pthread_t guiThread;
	GtkTextIter iter;
	time_t rawtime;
	struct tm* timeinfo;


	g_type_init();	
	dhcplog = gtk_text_buffer_new(NULL);
	gtk_text_buffer_get_iter_at_offset(dhcplog, &iter, 0);
	gtk_text_buffer_create_tag(dhcplog, "bold", "weight", PANGO_WEIGHT_BOLD, NULL);


        if (pthread_create(&guiThread, NULL, &gui_thread, NULL)){
        	printf("pthread_create failed\n");
	        return -1;
        }


	//setup UDP server
	if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
		printf("Error: socket()\n");
		return -1;
	}

	memset((void*) &serveraddr, 0, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = htonl(INADDR_BROADCAST);
	serveraddr.sin_port = htons((unsigned short) PORT);

	if(setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1){
		printf("Error: setsockopt(SO_REUSEADDR)\n");
		return -1;
	}

	if(setsockopt(fd, SOL_SOCKET, SO_BROADCAST, &yes, sizeof(int)) == -1){
		printf("Error: setsockopt(SO_BROADCAST)\n");
		return -1;
	}

	if(bind(fd, (struct sockaddr*) &serveraddr, sizeof(serveraddr))==-1){
		printf("Error: bind()\n");
		close(fd);
		return -1;
	}

	addrlen = sizeof(clientaddr);

	//wait for connections
	ptr = 0;
	while((recvBytes = recvfrom(fd, &buffer[ptr], DHCPLENGTH-ptr, 0, (struct sockaddr*) &clientaddr, &addrlen)) != -1) {
		if(recvBytes < DHCPLENGTH)
			if(recvBytes + ptr < DHCPLENGTH){
				ptr += recvBytes;
				continue;
			}
		if(recvBytes + ptr > DHCPLENGTH){ //Not a DHCP message
			ptr = 0;
			continue;
		}
		ptr = 0; //Already have the whole message: reset pointer
		
		dhcp = (struct dhcp_packet*)(&buffer);

		for(i=0;i<DHCP_OPTION_LEN;i++){
			if(i<4) //Skip the magic cookie
				continue;
			if(dhcp->options[i] == 0x35){ //Option: (53) DHCP Message Type
				length = dhcp->options[++i];
				if( (messageType = dhcp->options[i+1]) != 0x3)
					break;
				i+=length;
				continue;
			}else if(dhcp->options[i] == 0x32){ //Option: (50) Requested IP Address
				i++;
				requestedIP[0] = dhcp->options[++i];
				requestedIP[1] = dhcp->options[++i];
				requestedIP[2] = dhcp->options[++i];
				requestedIP[3] = dhcp->options[++i];
				continue;
			}else{
				i += dhcp->options[++i];//Skip through the other options
			}
									
		}

		if(messageType != 0x3) //If its not a Request then we dont want it
			continue;
		
		sprintf(clientHostname, "TODO");
		sprintf(clientIP, "%d.%d.%d.%d", requestedIP[0], requestedIP[1], requestedIP[2], requestedIP[3]);
		sprintf(clientMAC, "%02x:%02x:%02x:%02x:%02x:%02x", dhcp->chaddr[0], dhcp->chaddr[1],dhcp->chaddr[2],dhcp->chaddr[3],dhcp->chaddr[4],dhcp->chaddr[5]);
		reqIP = (requestedIP[0] << 24) | (requestedIP[1] << 16) | (requestedIP[2] << 8) | requestedIP[3];
		clientAddr.s_addr = htonl(reqIP);
		clientHName = gethostbyaddr(&clientAddr, sizeof(struct in_addr), AF_INET);

		if(clientHName)
			strncpy(clientHostname, clientHName->h_name, HOSTNAMELEN);
		else
			sprintf(clientHostname, "(none)");

		createNotification(clientIP, clientMAC, clientHostname);
	
		
		//Update the log
		time (&rawtime);
  		timeinfo = localtime (&rawtime);
		gtk_text_buffer_insert_with_tags_by_name(dhcplog, &iter , asctime(timeinfo) , -1, "bold", NULL);
		gtk_text_buffer_get_end_iter(dhcplog, &iter);
		gtk_text_buffer_insert_with_tags_by_name(dhcplog, &iter , "   Hostname: ", -1, "bold", NULL);
		gtk_text_buffer_insert_at_cursor(dhcplog, clientHostname, strlen(clientHostname));
		gtk_text_buffer_get_end_iter(dhcplog, &iter);
		gtk_text_buffer_insert_with_tags_by_name(dhcplog, &iter , " IP: ", -1, "bold", NULL);
		gtk_text_buffer_insert_at_cursor(dhcplog, clientIP, strlen(clientIP));
		gtk_text_buffer_get_end_iter(dhcplog, &iter);
		gtk_text_buffer_insert_with_tags_by_name(dhcplog, &iter , " MAC: ", -1, "bold", NULL);
		gtk_text_buffer_insert_at_cursor(dhcplog, clientMAC, strlen(clientMAC));
		gtk_text_buffer_insert_at_cursor(dhcplog, "\n", 1);
		gtk_text_buffer_get_end_iter(dhcplog, &iter);
	}

	//recvfrom returned -1 breaking the cycle

	close(fd);
	printf("Error: recvfrom()\n");
	return -1;
}
