import useSWR from "swr";
import { getApiRouteForCurrentDB } from "./arangoClient";

export const usePermissions = () => {
  const {
    data
  } = useSWR(
    `/user/${window.arangoHelper.getCurrentJwtUsername()}/database/${
      window.frontendConfig.db
    }`,
    path => getApiRouteForCurrentDB().get(path)
  );

  return data ? data.body.result : "none";
};

export const userIsAdmin = (permission: string) =>
  permission === "rw" || !window.frontendConfig.authenticationEnabled;

export const useIsAdminUser = () => {
  const permission = usePermissions();
  return userIsAdmin(permission);
};
