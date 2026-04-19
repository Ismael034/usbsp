import { createTheme } from "@mui/material/styles";

// Visual direction: "industrial console" (crisp surfaces, sharp-ish geometry, high contrast,
// minimal but intentional color).
const ink = "#111111";
const cobalt = "#0b5fff";
const ember = "#ff6b00";

export function buildTheme(mode = "light") {
  const isDark = mode === "dark";

  return createTheme({
    palette: {
      mode,
      primary: { main: cobalt },
      secondary: { main: ember },
      success: { main: "#16a34a" },
      warning: { main: "#f59e0b" },
      error: { main: "#dc2626" },
      info: { main: "#0284c7" },
      text: isDark
        ? { primary: "#f3f4f6", secondary: "#a3a3a3" }
        : { primary: ink, secondary: "#334155" },
      background: isDark
        ? { default: "#0f0f10", paper: "#18181b" }
        : { default: "#f6f6f2", paper: "#ffffff" },
      divider: isDark ? "rgba(255,255,255,0.10)" : "#d7dbe3"
    },

    // Kill the overly-rounded look.
    shape: { borderRadius: 8 },

    typography: {
      fontFamily: '"IBM Plex Sans","Segoe UI",system-ui,sans-serif',
      h1: { fontFamily: '"Space Grotesk","IBM Plex Sans","Segoe UI",system-ui,sans-serif' },
      h2: { fontFamily: '"Space Grotesk","IBM Plex Sans","Segoe UI",system-ui,sans-serif' },
      h3: { fontFamily: '"Space Grotesk","IBM Plex Sans","Segoe UI",system-ui,sans-serif' },
      h4: { fontFamily: '"Space Grotesk","IBM Plex Sans","Segoe UI",system-ui,sans-serif' },
      h5: { fontFamily: '"Space Grotesk","IBM Plex Sans","Segoe UI",system-ui,sans-serif' },
      h6: { fontFamily: '"Space Grotesk","IBM Plex Sans","Segoe UI",system-ui,sans-serif' },
      button: { textTransform: "none", fontWeight: 700, letterSpacing: 0.2 }
    },

    components: {
      MuiCssBaseline: {
        styleOverrides: {
          body: {
            color: isDark ? "#e2e8f0" : ink
          }
        }
      },

      MuiAppBar: {
        defaultProps: { elevation: 0, color: "transparent" },
        styleOverrides: {
          root: ({ theme }) => ({
            backgroundColor: theme.palette.background.paper,
            borderBottom: `1px solid ${theme.palette.divider}`
          })
        }
      },

      MuiContainer: {
        defaultProps: { maxWidth: "lg" }
      },

      MuiCard: {
        defaultProps: { variant: "outlined" },
        styleOverrides: {
          root: ({ theme }) => ({
            borderRadius: 10,
            borderColor: theme.palette.divider,
            boxShadow: "none"
          })
        }
      },

      MuiCardHeader: {
        styleOverrides: {
          root: { paddingBottom: 8 },
          title: { fontWeight: 800, letterSpacing: 0.2 },
          subheader: ({ theme }) => ({ color: theme.palette.text.secondary })
        }
      },

      MuiPaper: {
        styleOverrides: {
          root: {
            backgroundImage: "none"
          }
        }
      },

      MuiButton: {
        defaultProps: { disableElevation: true },
        styleOverrides: {
          root: {
            borderRadius: 10,
            paddingLeft: 14,
            paddingRight: 14
          },
          contained: { boxShadow: "none" },
          containedPrimary: ({ theme }) => ({
            backgroundColor:
              theme.palette.mode === "dark" ? theme.palette.primary.dark : theme.palette.primary.main,
            "&:hover": {
              backgroundColor:
                theme.palette.mode === "dark" ? theme.palette.primary.main : theme.palette.primary.dark
            }
          })
        }
      },

      MuiChip: {
        styleOverrides: {
          root: { borderRadius: 10, fontWeight: 750 }
        }
      },

      MuiOutlinedInput: {
        styleOverrides: {
          root: ({ theme }) => ({
            borderRadius: 10,
            backgroundColor: theme.palette.background.paper
          }),
          notchedOutline: ({ theme }) => ({ borderColor: theme.palette.divider })
        }
      }
    }
  });
}

export const theme = buildTheme("light");
