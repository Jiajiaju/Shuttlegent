// The parts of Tiptap the panel's message input uses, exposed as window.ShuttlegentTiptap
import { Editor, Extension } from '@tiptap/core';
import StarterKit from '@tiptap/starter-kit';
import { TaskList, TaskItem } from '@tiptap/extension-list';
import { Placeholder } from '@tiptap/extensions';
import { Markdown } from '@tiptap/markdown';

window.ShuttlegentTiptap = { Editor, Extension, StarterKit, TaskList, TaskItem, Placeholder, Markdown };
