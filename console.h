//#pragma once
// Ripped from the IMGUI example console
#include "cm_imgui.h"

namespace cm {
namespace pyrepl {
int executeAndPrint(const std::string& str);
}
}  // namespace cm

struct Console {
  char                  inputBuf[256];
  ImVector<char*>       items;
  bool                  scrollToBottom;
  ImVector<char*>       history;
  int                   historyPos;  // -1: new line, 0..history.Size-1 browsing history.
  ImVector<const char*> commands;

  float inputHeight = 30;
  bool  consoleOpen = false;

  Console() {
    clear();
    memset(inputBuf, 0, sizeof(inputBuf));
    historyPos = -1;
    commands.push_back("app.setFloat");
    commands.push_back("app.setBool");
    //commands.push_back("CLEAR");
    //commands.push_back("CLASSIFY");  // "classify" is here to provide an example of "C"+[tab] completing to "CL" and displaying matches.
    log("COLORMOTOR REPL");
  }

  ~Console() {
    clear();
    for (int i = 0; i < history.Size; i++)
      free(history[i]);
  }

  // Portable helpers
  static int Stricmp(const char* str1, const char* str2) {
    int d;
    while ((d = toupper(*str2) - toupper(*str1)) == 0 && *str1) {
      str1++;
      str2++;
    }
    return d;
  }
  static int Strnicmp(const char* str1, const char* str2, int n) {
    int d = 0;
    while (n > 0 && (d = toupper(*str2) - toupper(*str1)) == 0 && *str1) {
      str1++;
      str2++;
      n--;
    }
    return d;
  }
  static char* Strdup(const char* str) {
    size_t len  = strlen(str) + 1;
    void*  buff = malloc(len);
    return (char*)memcpy(buff, (const void*)str, len);
  }

  void clear() {
    for (int i = 0; i < items.Size; i++)
      free(items[i]);
    items.clear();
    scrollToBottom = true;
  }

  void flog(const char* fmt, ...) IM_FMTARGS(2) {
    char    buf[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, ARRAYSIZE(buf), fmt, args);
    buf[ARRAYSIZE(buf) - 1] = 0;
    va_end(args);
    items.push_back(Strdup(buf));
    scrollToBottom = true;
  }

  void log(const char* msg) {
    items.push_back(Strdup(msg));
    scrollToBottom = true;
  }

  void draw(float width, float height) {
    float H = ImGui::GetIO().DisplaySize.y;

    // if(consoleOpen)
    // {

    // }
    // else
    // {
    //     ImGui::SetNextWindowPos(ImVec2(0,H-inputHeight));
    // }

    static ImGuiTextFilter filter;

    ImVec2 size = ImVec2(width, height);
    if (consoleOpen) {
      ImGui::SetNextWindowPos(ImVec2(0, H - inputHeight - height));
      ImGui::SetNextWindowSize(size, ImGuiCond_Always);
      ImGui::SetNextWindowBgAlpha(0.5);
      ImGui::Begin("console", &consoleOpen, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);

      //ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoTitleBar|
      //ImGuiWindowFlags_NoScrollbar

      // ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0,0));
      // static ImGuiTextFilter filter;
      // filter.Draw("Filter (\"incl,-excl\") (\"error\")", 180);
      // ImGui::PopStyleVar();
      // ImGui::Separator();
      /*
            ImGui::BeginChild("ScrollingRegion", ImVec2(0,-ImGui::GetFrameHeightWithSpacing()), false, ImGuiWindowFlags_HorizontalScrollbar);
            */
      // Display every line as a separate entry so we can change their color or add custom widgets. If you only want raw text you can use ImGui::TextUnformatted(log.begin(), log.end());
      // NB- if you have thousands of entries this approach may be too inefficient and may require user-side clipping to only process visible items.
      // You can seek and display only the lines that are visible using the ImGuiListClipper helper, if your elements are evenly spaced and you have cheap random access to the elements.
      // To use the clipper we could replace the 'for (int i = 0; i < items.Size; i++)' loop with:
      //     ImGuiListClipper clipper(items.Size);
      //     while (clipper.Step())
      //         for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
      // However take note that you can not use this code as is if a filter is active because it breaks the 'cheap random-access' property. We would need random-access on the post-filtered list.
      // A typical application wanting coarse clipping and filtering may want to pre-compute an array of indices that passed the filtering test, recomputing this array when user changes the filter,
      // and appending newly elements as they are inserted. This is left as a task to the user until we can manage to improve this example code!
      // If your items are of variable size you may want to implement code similar to what ImGuiListClipper does. Or split your data into fixed height items to allow random-seeking into your list.
      ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 1));  // Tighten spacing
      for (int i = 0; i < items.Size; i++) {
        const char* item = items[i];
        if (!filter.PassFilter(item))
          continue;
        ImVec4 col = ImVec4(0.3f, 0.3f, 0.3f, 1.0f);  // A better implementation may store a type per-item. For the sample let's just parse the text.
        if (strstr(item, "[error]") || strstr(item, "Traceback"))
          col = ImColor(1.0f, 0.2f, 0.0f, 1.0f);
        else if (strncmp(item, "# ", 2) == 0)
          col = ImColor(0.0f, 0.5f, 1.0f, 1.0f);
        ImGui::PushStyleColor(ImGuiCol_Text, col);
        ImGui::TextUnformatted(item);
        ImGui::PopStyleColor();
      }
      if (scrollToBottom)
        ImGui::SetScrollHereY();
      scrollToBottom = false;
      ImGui::PopStyleVar();
      //ImGui::EndChild();
      ImGui::End();
    }

    // Input line
    size = ImVec2(width, inputHeight);
    ImGui::SetNextWindowPos(ImVec2(0, H - inputHeight), ImGuiCond_Always);
    ImGui::SetNextWindowSize(size, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(1.);
    static bool p_open = true;
    ImGui::Begin("console input", &p_open, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);

    if (ImGui::IconSelectable("e", consoleOpen))
      consoleOpen = !consoleOpen;
    ImGui::SameLine();

    ImGui::Text(">>");
    ImGui::SameLine();
    ImGui::PushItemWidth(width - 400);

    // Command-line
    if (ImGui::InputText(" ", inputBuf, ARRAYSIZE(inputBuf), ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackCompletion | ImGuiInputTextFlags_CallbackHistory, &textEditCallbackStub, (void*)this)) {
      char* input_end = inputBuf + strlen(inputBuf);
      while (input_end > inputBuf && input_end[-1] == ' ') input_end--;
      *input_end = 0;
      if (inputBuf[0])
        execCommand(inputBuf);
      strcpy(inputBuf, "");
    }

    // Demonstrate keeping auto focus on the input box
    if (ImGui::IsItemHovered() || (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && !ImGui::IsAnyItemActive() && !ImGui::IsMouseClicked(0)))
      ImGui::SetKeyboardFocusHere(-1);  // Auto focus previous widget

    ImGui::PopItemWidth();

    ImGui::SameLine();
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
    filter.Draw("Filter", 100);
    ImGui::PopStyleVar();

    //        ImGui::TextWrapped("This example implements a console with basic coloring, completion and history. A more elaborate implementation may want to store entries along with extra data such as timestamp, emitter, etc.");
    //        ImGui::TextWrapped("Enter 'HELP' for help, press TAB to use text completion.");

    // TODO: display items starting from the bottom
    ImGui::SameLine();
    if (ImGui::SmallButton("Clear")) clear();
    ImGui::SameLine();
    if (ImGui::SmallButton("Bottom")) scrollToBottom = true;
    //static float t = 0.0f; if (ImGui::GetTime() - t > 0.02f) { t = ImGui::GetTime(); log("Spam %f", t); }

    ImGui::End();
  }

  void execCommand(const char* command_line) {
    flog("# %s\n", command_line);
    consoleOpen = true;

    // Insert into history. First find match and delete it so it can be pushed to the back. This isn't trying to be smart or optimal.
    historyPos = -1;
    for (int i = history.Size - 1; i >= 0; i--)
      if (Stricmp(history[i], command_line) == 0) {
        free(history[i]);
        history.erase(history.begin() + i);
        break;
      }
    history.push_back(Strdup(command_line));

    cm::pyrepl::executeAndPrint(command_line);
  }

  static int textEditCallbackStub(ImGuiInputTextCallbackData* data)  // In C++11 you are better off using lambdas for this sort of forwarding callbacks
  {
    Console* console = (Console*)data->UserData;
    return console->textEditCallback(data);
  }

  int textEditCallback(ImGuiInputTextCallbackData* data) {
    //AddLog("cursor: %d, selection: %d-%d", data->CursorPos, data->SelectionStart, data->SelectionEnd);
    switch (data->EventFlag) {
      case ImGuiInputTextFlags_CallbackCompletion: {
        // Example of TEXT COMPLETION

        // Locate beginning of current word
        const char* word_end   = data->Buf + data->CursorPos;
        const char* word_start = word_end;
        while (word_start > data->Buf) {
          const char c = word_start[-1];
          if (c == ' ' || c == '\t' || c == ',' || c == ';')
            break;
          word_start--;
        }

        // Build a list of candidates
        ImVector<const char*> candidates;
        for (int i = 0; i < commands.Size; i++)
          if (Strnicmp(commands[i], word_start, (int)(word_end - word_start)) == 0)
            candidates.push_back(commands[i]);

        if (candidates.Size == 0) {
          // No match
          flog("No match for \"%.*s\"!\n", (int)(word_end - word_start), word_start);
        } else if (candidates.Size == 1) {
          // Single match. Delete the beginning of the word and replace it entirely so we've got nice casing
          data->DeleteChars((int)(word_start - data->Buf), (int)(word_end - word_start));
          data->InsertChars(data->CursorPos, candidates[0]);
          data->InsertChars(data->CursorPos, " ");
        } else {
          // Multiple matches. Complete as much as we can, so inputing "C" will complete to "CL" and display "CLEAR" and "CLASSIFY"
          int match_len = (int)(word_end - word_start);
          for (;;) {
            int  c                      = 0;
            bool all_candidates_matches = true;
            for (int i = 0; i < candidates.Size && all_candidates_matches; i++)
              if (i == 0)
                c = toupper(candidates[i][match_len]);
              else if (c == 0 || c != toupper(candidates[i][match_len]))
                all_candidates_matches = false;
            if (!all_candidates_matches)
              break;
            match_len++;
          }

          if (match_len > 0) {
            data->DeleteChars((int)(word_start - data->Buf), (int)(word_end - word_start));
            data->InsertChars(data->CursorPos, candidates[0], candidates[0] + match_len);
          }

          // List matches
          log("Possible matches:\n");
          for (int i = 0; i < candidates.Size; i++)
            flog("- %s\n", candidates[i]);
        }

        break;
      }
      case ImGuiInputTextFlags_CallbackHistory: {
        // Example of HISTORY
        const int prev_history_pos = historyPos;
        if (data->EventKey == ImGuiKey_UpArrow) {
          if (historyPos == -1)
            historyPos = history.Size - 1;
          else if (historyPos > 0)
            historyPos--;
        } else if (data->EventKey == ImGuiKey_DownArrow) {
          if (historyPos != -1)
            if (++historyPos >= history.Size)
              historyPos = -1;
        }

        // A better implementation would preserve the data on the current input line along with cursor position.
        if (prev_history_pos != historyPos) {
          data->CursorPos = data->SelectionStart = data->SelectionEnd = data->BufTextLen = (int)snprintf(data->Buf, (size_t)data->BufSize, "%s", (historyPos >= 0) ? history[historyPos] : "");
          data->BufDirty                                                                 = true;
        }
      }
    }
    return 0;
  }
};
