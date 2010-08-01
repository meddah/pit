#if !defined(__MODELS_H__)
#define __MODELS_H__

typedef struct _Header {
    char signature[3];
    char schema_version;
    char reserved[16];
} Header, *PHeader;
    
typedef struct _Project {
    ulong  id;
    char   username[32];            /* User the project belongs to. */
    char   name[128];               /* Project name. */
    char   status[16];              /* Project status. */
    ulong  number_of_tasks;         /* Number of tasks for the project. */
    ulong  created_by;              /* Who created the project? */
    ulong  updated_by;              /* Who last updated the project? */
    time_t created_at;              /* When the project was created? */
    time_t updated_at;              /* When the project was last updated? */
} Project, *PProject;

typedef struct _Task {
    ulong  id;
    ulong  project_id;              /* Which project the task belongs to? */
    char   username[32];            /* User the task belongs to. */
    char   name[128];               /* Task name. */
    char   status[16];              /* Task status. */
    char   priority[16];            /* Task priority. */
    time_t date;                    /* Generic date/time, ex: task deadline. */
    time_t time;                    /* Generic time, ex: time spent on the task. */
    ulong  number_of_notes;         /* Number of notes for the task. */
    time_t created_at;              /* When the task was created? */
    time_t updated_at;              /* When the task was last updated? */
} Task, *PTask;

typedef struct _Note {
    ulong  id;
    ulong  task_id;                 /* Task the note belongs to. */
    char   username[32];            /* User the note belongs to. */
    char   message[255];            /* The body of the note. */
    time_t created_at;              /* When the note was created? */
    time_t updated_at;              /* When the note was last updated? */
} Note, *PNote;

typedef struct _Action {
    ulong  subject_id;              /* Reference to the specific Project, Task, or Note. */
    char   subject[16];             /* Project, Task, or Note. */
    char   username[32];            /* Who added the log message? */
    char   message[255];            /* Log message. */
    time_t created_at;              /* When log message was added? */    
} Action, *PAction;

#endif