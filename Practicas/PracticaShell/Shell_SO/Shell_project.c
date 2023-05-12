/**
Santiago Ponce Arrocha - Grupo C
UNIX Shell Project

Sistemas Operativos
Grados I. Informatica, Computadores & Software
Dept. Arquitectura de Computadores - UMA

Some code adapted from "Fundamentos de Sistemas Operativos", Silberschatz et al.

To compile and run the program:
   $ gcc Shell_project.c job_control.c -o Shell
   $ ./Shell
	(then type ^D to exit program)

**/

#include "job_control.h" // remember to compile with module job_control.c
#include <string.h>

#define MAX_LINE 256 /* 256 chars per line, per command, should be enough. */

job *tareas; // lista para que el manejador pueda acceder.

void manejador(int senial)
{ // recibe un entero, que es el numero de la senial (17=SIGCHLD)
	block_SIGCHLD();
	/*
	SIGCHLD() es un manejador que recorre todos los jobs en bg y suspendidos a ver que les ha pasado
	-Si EXITED= quita de la lista
	-Si cambian de estado= cambia el estado del job correspondiente
	*/
	job *item;
	int status, info;
	int pid_wait = 0;
	enum status status_res;

	for (int i = 1; i <= list_size(tareas); i++)
	{
		item = get_item_bypos(tareas, i); // coger un job dependiendo de la posicion de la lista "item".
		pid_wait = waitpid(item->pgid, &status, WUNTRACED | WNOHANG | WCONTINUED);

		if (pid_wait == item->pgid)
		{												// si ha pasado algo al proceso (muerto, suspendido,...)
			status_res = analyze_status(status, &info); // EXITED,CONTINUED,SIGNALED,SUSPENDED

			if (status_res == SUSPENDED)
			{
				printf("\nComando %s en segundo plano se ha suspendido con PID %d\n", item->command, item->pgid);
				item->state = STOPPED;
			}
			else if (status_res == EXITED || status_res == SIGNALED)
			{
				printf("\nComando %s ejecutando en background con PID %d ha acabado su ejecucion\n", item->command, item->pgid);
				delete_job(tareas, item);
				i--;
			}
			else if (status_res == CONTINUED)
			{
				item->state = BACKGROUND;
			}
		}
	}
	unblock_SIGCHLD();
}
// -----------------------------------------------------------------------
//                            MAIN
// -----------------------------------------------------------------------

int main(void)
{
	char inputBuffer[MAX_LINE]; /* buffer to hold the command entered */
	int background;				/* equals 1 if a command is followed by '&' */
	char *args[MAX_LINE / 2];	/* command line (of 256) has max of 128 arguments */
	// probably useful variables:
	int pid_fork, pid_wait; /* pid for created and waited process */
	int status;				/* status returned by wait */
	enum status status_res; /* status processed by analyze_status() */
	int info;				/* info processed by analyze_status() */

	int foreground;
	job *item;
	tareas = new_list("miLista");
	signal(SIGCHLD, manejador);

	while (1) /* Program terminates normally inside get_command() after ^D is typed*/
	{

		foreground = 0;

		ignore_terminal_signals(); // ingora las seniales ^C y ^Z que son incomodas

		printf("COMMAND->");
		fflush(stdout);
		get_command(inputBuffer, MAX_LINE, args, &background);
		/*
		Obtiene el siguiente comando.
		imputBuffer --> array donde se almacena lo que introduzca el usuario
		MAX_LINE--> tamanio maximo del array
		args(array de strings)--> args[0]primer comando / args[1] segundo comando / ...
		*/

		if (args[0] == NULL)
			continue; // si se introduce un comando vacio, no se hace nada

		// Parte 2 ---> Comando interno "cd" :D
		if (!strcmp(args[0], "cd"))
		{
			// Compara la cadena char en la posicion 0 del array "args". Si se cumple que son iguales, ejecuta el comando interno "cd"
			chdir(args[1]);
			continue;
		}
		/*
		NOTA!!
		Que la comparacion sea diferente (!), es por una discordancia. Cuando los dos son iguales, devuelve 0 la comparacion,
		pero estariamos diciendo mas abajo que el comando es erroneo. Por eso, si se cumple, valdria algo diferente de 0,
		y por tanto, entraria como un comando en el primer "if".
		*/

		if (!strcmp(args[0], "jobs"))
		{ // imprime los procesos suspendidos y en segundo plano.
			block_SIGCHLD();
			if (list_size(tareas) > 0)
			{
				print_job_list(tareas);
			}
			else
			{
				printf("No Jobs in tarea\n");
			}
			unblock_SIGCHLD();
			continue;
		}

		if (!strcmp(args[0], "bg"))
		{
			int pos = 1;
			if (args[1] != NULL)
			{
				pos = atoi(args[1]); //"atoi" convierte pos(String) a un entero(int).
			}
			block_SIGCHLD();
			if (pos <= 0 || pos > list_size(tareas))
			{
				printf("Numero de job no valido\n");
				unblock_SIGCHLD();
				continue;
			}
			item = get_item_bypos(tareas, pos);

			// si el argumento esta dentro de la lista y esta detenido, pasa a segundo plano
			if ((item != NULL) && (item->state == STOPPED))
			{
				item->state = BACKGROUND;
				killpg(item->pgid, SIGCONT); // manda una senial al miembro del grupo especifico para que su proceso pase de detenido a reaunado.
			}
			else if (item == NULL)
			{
				printf("No elements in position %i \n", pos);
			}
			unblock_SIGCHLD();
			continue;
		}

		char *cmd = args[0];
		int fgFree = 0;

		if (!strcmp(args[0], "fg"))
		{
			int pos = 1;
			foreground = 1; // para determinar mas adelante si el proceso esta en primer plano y entrar o no.
			if (args[1] != NULL)
			{
				pos = atoi(args[1]); //"atoi" convierte pos(String) a un entero(int).
			}
			block_SIGCHLD();
			if (pos <= 0 || pos > list_size(tareas))
			{
				printf("Numero de job no valido\n");
				unblock_SIGCHLD();
				continue;
			}
			item = get_item_bypos(tareas, pos);
			if ((item != NULL))
			{							  // no ponermos segunda condicion, puesto que los procesos en lista estan suspendidos o en segundo plano.
				set_terminal(item->pgid); // cedemos el terminal al proceso.

				if (item->state == STOPPED)
				{								 // en caso de que este suspendido, enviamos la senial SIGCONT.
					killpg(item->pgid, SIGCONT); // manda una senial al miembro del grupo especifico para que su proceso pase de detenido a reaunado.
				}
				fgFree = 1;
				pid_fork = item->pgid;
				cmd = strdup(item->command);
				delete_job(tareas, item); // una vez sacamos el proceso de la lista de suspendidos, eliminamos el proceso de la lista, puesto que se encontraria ahora en primer plano.
				unblock_SIGCHLD();
			}
			else
			{
				unblock_SIGCHLD();
				continue;
			}
		}

		if (foreground == 0)
		{ // si foreground no es igual a 1, se crea un nuevo proceso hijo. Si es igual a 1, se inicializa las codiciones con dicho proceso como padre.
			pid_fork = fork();
		}

		/*COMANDOS EXTERNOS*/
		// creo un hijo
		// pid_fork=fork(); CAMBIADO PARA IMPLEMENTAR LA FUNCIONE "fg".
		/*
		Crea una copia exacta del codigo de memoria(pila, monticulo...) y lo pone en otro lado de la memoria.

		La funcion fork --> Al proceso padre le asigna "pid_fork" el PID del proceso hijo.
		Mientras que al proceso hijo, el PID que se le asignara valdra 0, permitiendonos diferencia cual es cada proceso.
		*/
		if (pid_fork > 0) // padre = shell
		{
			if (background == 0)
			{
				set_terminal(pid_fork);
				waitpid(pid_fork, &status, WUNTRACED);		//(luego de execvp() del hijo ) El padre se queda estancado hasta que el hijo acabe. WUNTRACED=el proceso padre deja de seguir al hijo (esta suspendido).
				set_terminal(getpid());						// recupero la terminal, despues del exit() del hijo y me devuelve el PID del proceso actual.
				status_res = analyze_status(status, &info); // coge la variable status, mira lo que tiene, y en funcion de los que tenga, mete en status_res lo que haya recogido ("enum status").

				if (status_res == SUSPENDED)
				{
					item = new_job(pid_fork, cmd, STOPPED);
					if (fgFree)
					{
						free(cmd); // libera memoria
					}
					// block_SIGCHLD(); //seccion critica libre de SIGCHLD(bit 17).
					add_job(tareas, item);
					printf("\nProcess in background suspended\n");
					// unblock_SIGCHLD(); //permitir los SIGCHLD pendientes.
				}
				else if (status_res == EXITED)
				{
					printf("\nProcess in background died by itself\n");
				}
				else if (status_res == SIGNALED)
				{
					printf("\nProcess in background was murdered by a signal\n");
				}
				else if (status_res == CONTINUED)
				{
					printf("\nProcess in background continue\n");
				}
				// Foreground pid: 5615, command: ls, Exited, info: 0
				printf("\nForeground pid: %d, command: %s, %d, Info: %d \n", pid_fork, args[0], status_res, info);
				foreground = 0; // reseteo la variable de linea 122
			}
			else
			{
				item = new_job(pid_fork, args[0], BACKGROUND);
				// block_SIGCHLD();
				add_job(tareas, item);
				printf("\nBackground job running... pid: %d, command: %s \n", pid_fork, args[0]);
				// unblock_SIGCHLD();
			}
		}
		else
		{								 // el hijo
			new_process_group(getpid()); // asigno un PID al hijo diferente del padre, para que el terminal sepa donde tiene que ir.
			if (background == 0)
			{
				set_terminal(getpid()); // asigno el terminal al proceso hijo.
			}
			restore_terminal_signals();
			execvp(args[0], args); // sustituye todo el codigo por el del comando que introduzcamos.
			// si el comando introducido no existe, no va a cargar ningun codigo,, y "execvp() no cambiara el codigo".
			// en caso de fallom carga las lineas de abajo automaticamente
			printf("\nError, command not found: %s \n", args[0]);
			exit(-1);
		}
	} // end while
}
