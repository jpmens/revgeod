#include <stdio.h>
#include <sys/types.h>

void uptime(time_t seconds, char *dest, size_t destlen)
{
	int days, hours, mins;

	days = seconds / 86400;
	hours = (seconds / 3600) - (days * 24);
	mins = (seconds / 60) - (days * 24 * 60) - (hours * 60);

	snprintf(dest, destlen, "%d days, %d hours, %d mins", days, hours, mins);
}
