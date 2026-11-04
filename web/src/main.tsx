import { StrictMode } from 'react';
import { createRoot } from 'react-dom/client';
import { createTheme, MantineProvider } from '@mantine/core';
import { Notifications } from '@mantine/notifications';
import '@mantine/core/styles.css';
import '@mantine/notifications/styles.css';
import { App } from './App';
import { I18nProvider } from './i18n';

const theme = createTheme({ primaryColor: 'blue', defaultRadius: 'md' });

createRoot(document.getElementById('root')!).render(
  <StrictMode>
    <MantineProvider theme={theme} defaultColorScheme="auto">
      <Notifications position="bottom-center" />
      <I18nProvider>
        <App />
      </I18nProvider>
    </MantineProvider>
  </StrictMode>,
);
